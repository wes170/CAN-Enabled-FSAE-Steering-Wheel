/* display_wheel.c — JDI LPM013M126A driver.
 *
 * Structure: a full 176x176 framebuffer at 3 bits per pixel (11.6 kB) plus a
 * dirty-line bitmap. Only changed lines are transmitted, which on a memory-in-
 * pixel panel means a typical update costs a handful of lines rather than a
 * frame. At the 2 MHz SCLK ceiling a single line is 66 bytes + 4 bytes of
 * overhead = 280 µs, so a full-screen redraw is ~49 ms. Updating only the
 * dirty lines is not an optimisation, it is what keeps the display responsive.
 */

#include "display_wheel.h"
#include <string.h>

#ifdef BOARD_WHEEL

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

static uint8_t  s_fb[LCD_HEIGHT][LCD_LINE_BYTES];
static uint8_t  s_dirty[(LCD_HEIGHT + 7u) / 8u];
static uint32_t s_extcomin_last_ms;
static bool     s_extcomin_level;
static bool     s_visible;

/* ---------------------------------------------------------------------------
 * Pure packing / command building
 * ------------------------------------------------------------------------ */

void lcd_line_command(uint8_t line, uint8_t out[2])
{
    if (!out) { return; }
    /* [M0 M1 M2 M3 M4 M5 AG9 AG8] then [AG7..AG0].
     * AG9/AG8 come from the address rather than being hard-coded zero: they
     * are always zero for a 176-line panel, and hard-coding that is a silent
     * bug the first time someone reuses this for a taller one. */
    out[0] = (uint8_t)(LCD_M0_DATA_UPDATE | ((line >> 8) & 0x03u));
    out[1] = (uint8_t)(line & 0xFFu);
}

/*  Pixels are packed 3 bits each, R-G-B, pixel 0 first, MSB-first within the
 *  byte stream — so pixel x occupies bits [3x .. 3x+2] counting from the MSB
 *  of byte 0. Pixels straddle byte boundaries (3 does not divide 8), which is
 *  the fiddly part and the reason this is a tested function rather than
 *  open-coded at each call site. */
void lcd_line_set_pixel(uint8_t *line_buf, uint8_t x, lcd_colour_t c)
{
    if (!line_buf || x >= LCD_WIDTH) { return; }

    const unsigned bit = (unsigned)x * 3u;      /* first bit index, from MSB */
    const unsigned v   = (unsigned)c & 0x07u;

    for (unsigned i = 0; i < 3u; ++i) {
        const unsigned b     = bit + i;
        const unsigned byte  = b >> 3;
        const unsigned shift = 7u - (b & 7u);   /* MSB-first within the byte */
        const unsigned mask  = 1u << shift;
        /* Colour bit i counting from the MSB of the 3-bit value: R, then G,
         * then B, matching the spec's "Red-Green-Blue (3bit)" order. */
        if (v & (1u << (2u - i))) { line_buf[byte] |= (uint8_t)mask; }
        else                      { line_buf[byte] &= (uint8_t)~mask; }
    }
}

lcd_colour_t lcd_line_get_pixel(const uint8_t *line_buf, uint8_t x)
{
    if (!line_buf || x >= LCD_WIDTH) { return LCD_BLACK; }

    const unsigned bit = (unsigned)x * 3u;
    unsigned v = 0u;
    for (unsigned i = 0; i < 3u; ++i) {
        const unsigned b     = bit + i;
        const unsigned shift = 7u - (b & 7u);
        if (line_buf[b >> 3] & (1u << shift)) { v |= (1u << (2u - i)); }
    }
    return (lcd_colour_t)v;
}

/* ---------------------------------------------------------------------------
 * Framebuffer
 * ------------------------------------------------------------------------ */

static void mark_dirty(uint8_t y)
{
    if (y < LCD_HEIGHT) { s_dirty[y >> 3] |= (uint8_t)(1u << (y & 7u)); }
}

static bool is_dirty(uint8_t y)
{
    return (y < LCD_HEIGHT) && (s_dirty[y >> 3] & (1u << (y & 7u))) != 0u;
}

void display_wheel_set_pixel(uint8_t x, uint8_t y, lcd_colour_t c)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) { return; }
    if (lcd_line_get_pixel(s_fb[y], x) == c) { return; }   /* no-op, stays clean */
    lcd_line_set_pixel(s_fb[y], x, c);
    mark_dirty(y);
}

void display_wheel_clear(void)
{
    memset(s_fb, 0, sizeof s_fb);
    memset(s_dirty, 0xFF, sizeof s_dirty);
}

/* ---------------------------------------------------------------------------
 * EXTCOMIN — the destruction watchdog
 * ------------------------------------------------------------------------ */

void display_wheel_task_1ms(uint32_t now_ms)
{
    /* Toggle at 2 x the required rate so a full high/low cycle completes each
     * period: at 1 Hz that is a transition every 500 ms. */
    const uint32_t half_period_ms = 500u / LCD_EXTCOMIN_TOGGLE_HZ;

    if ((uint32_t)(now_ms - s_extcomin_last_ms) >= half_period_ms) {
        s_extcomin_last_ms = now_ms;
        s_extcomin_level   = !s_extcomin_level;
#ifndef FIRMWARE_HOST_BUILD
        if (s_extcomin_level) { LCD_EXTCOMIN_PORT->BSRR = (1u << LCD_EXTCOMIN_PIN); }
        else                  { LCD_EXTCOMIN_PORT->BSRR = (1u << (LCD_EXTCOMIN_PIN + 16u)); }
#endif
    }
}

bool display_wheel_extcomin_healthy(uint32_t now_ms)
{
    /* If more than three half-periods have passed without a toggle, the task
     * has stalled and the panel is accumulating DC bias. Report it loudly —
     * this is worth an on-screen warning and a log entry, because the damage
     * is cumulative and permanent and nothing else will announce it. */
    const uint32_t half_period_ms = 500u / LCD_EXTCOMIN_TOGGLE_HZ;
    return (uint32_t)(now_ms - s_extcomin_last_ms) < (half_period_ms * 3u);
}

/* ---------------------------------------------------------------------------
 * Init and flush
 * ------------------------------------------------------------------------ */

void display_wheel_init(void)
{
    memset(s_fb, 0, sizeof s_fb);
    memset(s_dirty, 0, sizeof s_dirty);
    s_extcomin_last_ms = 0u;
    s_extcomin_level   = false;
    s_visible          = false;

    /* Power sequence, §4.3:
     *   T2  pixel memory initialisation, >= 1 ms  (all-clear, or write black)
     *   T3  internal latch release,       >= 30 us
     *   T4  COM polarity initialisation,  >= 30 us
     * then DISP high and EXTCOMIN running.
     *
     * The hardware side of the same sequence is already satisfied by the
     * schematic: VDD and VDDA are both `+3V3`, so they rise together, which
     * meets the spec's "VDD and VDDA should rise simultaneously or VDD should
     * rise first". Giving VDDA its own filtered rail would break that. */
#ifndef FIRMWARE_HOST_BUILD
    lcd_send_all_clear();
    delay_us(1000u);                     /* T2 */
    delay_us(LCD_T3_LATCH_RELEASE_US);   /* T3 */
    delay_us(LCD_T4_COM_INIT_US);        /* T4 */
    display_wheel_set_visible(true);
#endif
}

void display_wheel_set_visible(bool on)
{
    s_visible = on;
#ifndef FIRMWARE_HOST_BUILD
    if (on) { LCD_DISP_PORT->BSRR = (1u << LCD_DISP_PIN); }
    else    { LCD_DISP_PORT->BSRR = (1u << (LCD_DISP_PIN + 16u)); }
#endif
}

void display_wheel_flush(void)
{
#ifndef FIRMWARE_HOST_BUILD
    for (unsigned y = 0; y < LCD_HEIGHT; ++y) {
        if (!is_dirty((uint8_t)y)) { continue; }

        uint8_t cmd[2];
        /* Framebuffer row y is panel line y+1: addressing is ONE-based. */
        lcd_line_command((uint8_t)(y + LCD_FIRST_LINE), cmd);

        lcd_scs_high();                  /* ACTIVE HIGH */
        lcd_spi_write(cmd, 2u);
        lcd_spi_write(s_fb[y], LCD_LINE_BYTES);
        /* 16 clocks of dummy after the last data, per §6.1. */
        static const uint8_t dummy[2] = { 0u, 0u };
        lcd_spi_write(dummy, 2u);
        lcd_scs_low();                   /* clears M0/M2 */
    }
#endif
    memset(s_dirty, 0, sizeof s_dirty);
}

/* Test-only. */
const uint8_t *display_wheel_debug_line(uint8_t y)
{
    return (y < LCD_HEIGHT) ? s_fb[y] : (const uint8_t *)0;
}

bool display_wheel_debug_dirty(uint8_t y) { return is_dirty(y); }
bool display_wheel_debug_visible(void)    { return s_visible; }

#endif /* BOARD_WHEEL */
