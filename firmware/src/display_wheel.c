/* display_wheel.c — Sharp LS027B7DH01 driver.
 *
 * Structure: a full 400x240 framebuffer at 1 bit per pixel (12,000 bytes) plus
 * a dirty-line bitmap. Only changed lines are transmitted, which on a
 * memory-in-pixel panel means a typical update costs a handful of lines rather
 * than a frame. A single line is 50 data bytes + 4 bytes of overhead = 432
 * clocks, so a full-screen redraw is ~104 ms at 1 MHz and ~52 ms at the 2 MHz
 * ceiling. Updating only the dirty lines is not an optimisation, it is what
 * keeps the display responsive at all.
 *
 * On top of that sits a periodic FULL rewrite (LCD_ANTI_STICK_REFRESH_MS) which
 * is a requirement of this panel rather than a nicety -- see display_wheel.h.
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
static uint32_t s_refresh_last_ms;
static bool     s_extcomin_level;
static bool     s_visible;

/* ---------------------------------------------------------------------------
 * Pure packing / command building
 * ------------------------------------------------------------------------ */

uint8_t lcd_reverse8(uint8_t v)
{
    /* Standard three-step swap. Kept branch-free and obvious rather than using
     * a 256-byte table: this runs once per line, not once per pixel. */
    v = (uint8_t)(((v & 0xF0u) >> 4) | ((v & 0x0Fu) << 4));
    v = (uint8_t)(((v & 0xCCu) >> 2) | ((v & 0x33u) << 2));
    v = (uint8_t)(((v & 0xAAu) >> 1) | ((v & 0x55u) << 1));
    return v;
}

void lcd_line_command(uint8_t line, uint8_t out[2])
{
    if (!out) { return; }

    /* Byte 0: [M0 M1 M2 + 5 dummy], M0 first on the wire = bit 7.
     * M1 is left low deliberately -- it is the frame-inversion flag and only
     * has meaning when EXTMODE = L. This board straps EXTMODE high and drives
     * COM from EXTCOMIN, so setting M1 here would be a no-op that misleads the
     * next reader into thinking inversion is handled in software. */
    out[0] = (uint8_t)LCD_M0_DATA_UPDATE;

    /* Byte 1: the 8-bit gate address, AG0 FIRST. See the §6-6 table quoted in
     * display_wheel.h -- this reversal is the whole reason the JDI driver's
     * address code could not be carried over. */
    out[1] = lcd_reverse8(line);
}

void lcd_all_clear_command(uint8_t out[2])
{
    if (!out) { return; }
    /* §6-5-4: M0 = L, M2 = H, then >= 13 dummy clocks. Two bytes gives 3 mode
     * bits + 13 dummy exactly. */
    out[0] = (uint8_t)LCD_M2_ALL_CLEAR;
    out[1] = 0u;
}

/*  Pixels are 1 bit each, pixel 0 first, MSB-first within the byte stream --
 *  so pixel x is bit (7 - x % 8) of byte (x / 8). 400 divides evenly by 8, so
 *  unlike the old 3-bit packing no pixel straddles a byte boundary and the
 *  arithmetic is exact.
 *
 *  ⚠ ONE BENCH CHECK: the datasheet's timing chart shows D1..D400 in order but
 *  does not state the bit order within a byte at a resolution that can be read
 *  from the PDF. If the first image comes out mirrored in 8-pixel blocks, this
 *  shift is the only thing to change -- flip `7u - (x & 7u)` to `(x & 7u)`.
 *  Recorded as a bring-up observation rather than guessed at silently. */
void lcd_line_set_pixel(uint8_t *line_buf, uint16_t x, lcd_colour_t c)
{
    if (!line_buf || x >= LCD_WIDTH) { return; }

    const unsigned byte  = (unsigned)x >> 3;
    const unsigned shift = 7u - ((unsigned)x & 7u);
    const uint8_t  mask  = (uint8_t)(1u << shift);

    if (c == LCD_WHITE) { line_buf[byte] |= mask; }
    else                { line_buf[byte] = (uint8_t)(line_buf[byte] & ~mask); }
}

lcd_colour_t lcd_line_get_pixel(const uint8_t *line_buf, uint16_t x)
{
    if (!line_buf || x >= LCD_WIDTH) { return LCD_BLACK; }

    const unsigned byte  = (unsigned)x >> 3;
    const unsigned shift = 7u - ((unsigned)x & 7u);
    return (line_buf[byte] & (1u << shift)) ? LCD_WHITE : LCD_BLACK;
}

/* ---------------------------------------------------------------------------
 * Framebuffer
 * ------------------------------------------------------------------------ */

static void mark_dirty(uint16_t y)
{
    if (y < LCD_HEIGHT) { s_dirty[y >> 3] |= (uint8_t)(1u << (y & 7u)); }
}

static bool is_dirty(uint16_t y)
{
    return (y < LCD_HEIGHT) && (s_dirty[y >> 3] & (1u << (y & 7u))) != 0u;
}

void display_wheel_set_pixel(uint16_t x, uint16_t y, lcd_colour_t c)
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
 * EXTCOMIN — the destruction watchdog — and the anti-sticking refresh
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

    /* Anti-sticking / static-discharge recovery. Marking every line dirty makes
     * the next flush a full rewrite; it does NOT transmit from here, because
     * this runs on the 1 ms tick and a full frame is ~104 ms of SPI. */
    if ((uint32_t)(now_ms - s_refresh_last_ms) >= LCD_ANTI_STICK_REFRESH_MS) {
        s_refresh_last_ms = now_ms;
        memset(s_dirty, 0xFF, sizeof s_dirty);
    }
}

bool display_wheel_extcomin_healthy(uint32_t now_ms)
{
    /* If more than three half-periods have passed without a toggle, the task
     * has stalled and the panel is accumulating DC bias. Report it loudly —
     * this is worth an on-screen warning and a log entry, because the damage is
     * cumulative and permanent and nothing else will announce it. */
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
    s_refresh_last_ms  = 0u;
    s_extcomin_level   = false;
    s_visible          = false;

    /* Power sequence, §6-2:
     *   T2  pixel memory initialisation, >= 1 ms  (all-clear, or write white)
     *   T3  TCOM latch release,          >= 30 us
     *   T4  TCOM polarity initialisation,>= 30 us
     * and ※1: with DISP and EXTCOMIN started together, allow >= 30 us before
     * SCS starts up.
     *
     * The hardware half of the same sequence is satisfied by the schematic:
     * VDD and VDDA are both `+5V`, so they rise together, which meets the
     * spec's "VDD and VDDA should rise simultaneously or VDD should rise
     * first". Giving VDDA its own separate or filtered rail would break that. */
#ifndef FIRMWARE_HOST_BUILD
    lcd_send_all_clear();
    delay_us(1000u * LCD_T2_MEMORY_INIT_MS);   /* T2 */
    delay_us(LCD_T3_LATCH_RELEASE_US);         /* T3 */
    delay_us(LCD_T4_COM_INIT_US);              /* T4 */
    display_wheel_set_visible(true);
    delay_us(LCD_T_SCS_AFTER_DISP_US);         /* ※1 before the first SCS */
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
        if (!is_dirty((uint16_t)y)) { continue; }

        uint8_t cmd[2];
        /* Framebuffer row y is panel line y+1: gate addressing is ONE-based. */
        lcd_line_command((uint8_t)(y + LCD_FIRST_LINE), cmd);

        lcd_scs_high();                  /* ACTIVE HIGH */
        lcd_spi_write(cmd, 2u);
        lcd_spi_write(s_fb[y], LCD_LINE_BYTES);
        /* 16 clocks of dummy after the last data, per §6-5-1. */
        static const uint8_t dummy[2] = { 0u, 0u };
        lcd_spi_write(dummy, 2u);
        lcd_scs_low();                   /* clears M0/M2 */
    }
#endif
    memset(s_dirty, 0, sizeof s_dirty);
}

/* Test-only. */
const uint8_t *display_wheel_debug_line(uint16_t y)
{
    return (y < LCD_HEIGHT) ? s_fb[y] : (const uint8_t *)0;
}

bool display_wheel_debug_dirty(uint16_t y) { return is_dirty(y); }
bool display_wheel_debug_visible(void)     { return s_visible; }

#endif /* BOARD_WHEEL */
