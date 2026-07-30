/* display_wheel.h — JDI LPM013M126A, 176x176, 8 colours, reflective MIP.
 *
 * A memory-in-pixel panel holds its image with no refresh, so there is no
 * frame loop here — write the lines that changed and stop. Ambient light
 * IMPROVES its contrast instead of washing it out, which is why it is on a
 * steering wheel in direct sun.
 *
 * ⚠ THE ONE THING THAT DESTROYS THIS PANEL: EXTCOMIN must keep toggling.
 * The liquid crystal needs its drive polarity inverted periodically; left at
 * one polarity, a DC bias builds across the cell and damages it PERMANENTLY.
 * That is a hardware destruction path, not a picture-quality issue, and it is
 * invisible until it is too late.
 *
 * Every constant below is read from the JDI specification Ver.01 (§6.1 single
 * line update mode, §6.8 all clear mode, §4.3 power sequence).
 */
#ifndef DISPLAY_WHEEL_H
#define DISPLAY_WHEEL_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_WHEEL

#define LCD_WIDTH   176u
#define LCD_HEIGHT  176u

/* 3 bits per pixel; 176 x 3 = 528 bits = exactly 66 bytes per line. */
#define LCD_LINE_BYTES ((LCD_WIDTH * 3u) / 8u)

typedef enum {
    LCD_BLACK   = 0x0u,
    LCD_BLUE    = 0x1u,
    LCD_GREEN   = 0x2u,
    LCD_CYAN    = 0x3u,
    LCD_RED     = 0x4u,
    LCD_MAGENTA = 0x5u,
    LCD_YELLOW  = 0x6u,
    LCD_WHITE   = 0x7u
} lcd_colour_t;

/*  ═══ WIRE FORMAT (JDI spec §6.1, read off the timing chart) ═══
 *
 *  SI carries, in this order, MSB first throughout:
 *
 *    M0 M1 M2 M3 M4 M5   6 clocks   "mode select period"
 *    AG9 ... AG0        10 clocks   "gate line address select period"
 *    D1R D1G D1B D2R…   3 bits per pixel, pixel 1 first, R-G-B within a pixel
 *    dummy              16 clocks   "data transfer period"
 *
 *  ⚠ THE ADDRESS IS MSB-FIRST AND 10 BITS WIDE. It is tempting to reuse the
 *  Sharp memory-LCD driver everyone has lying around, which sends an 8-bit
 *  line address BIT-REVERSED. Do that here and the panel works perfectly —
 *  it just writes the wrong lines, which reads as a layout bug rather than a
 *  bus bug and can cost a long time. AG9/AG8 live in the FIRST byte.
 *
 *  Mode table, §6.1 single line update, 3-bit data:
 *      M0 = H (data update)   M1 = don't care (EXTMODE is H, so M1 is ignored)
 *      M2 = L (not all-clear) M3 = L, M4 = L (3-bit data mode)   M5 = don't care
 *
 *  Mode table, §6.8 all clear:  M0 = L, M2 = H.
 *
 *  So the first byte is  [M0 M1 M2 M3 M4 M5 AG9 AG8]  and the second is
 *  [AG7…AG0]. For this 176-line panel AG9/AG8 are always 0, but they are
 *  written from the address rather than hard-coded, because a hard-coded 0 is
 *  a silent bug the day anyone reuses this for a taller panel. */
#define LCD_M0_DATA_UPDATE 0x80u   /* bit 7 */
#define LCD_M2_ALL_CLEAR   0x20u   /* bit 5 */

/*  Line addressing is ONE-BASED: the spec gives "m : Number of vertical line,
 *  1 to 176". Line 0 does not exist. */
#define LCD_FIRST_LINE 1u

/*  SCLK ceiling. Datasheet maximum is 2.00 MHz (1.00 MHz typical). SPI1 hangs
 *  off a 170 MHz bus and will clock far faster if the prescaler is chosen for
 *  convenience rather than against this number. */
#define LCD_SCLK_MAX_HZ 2000000u

/*  ⚠ SCS IS ACTIVE HIGH — the opposite of nearly every other SPI peripheral.
 *  It therefore CANNOT be SPI1's hardware NSS, which is active low. It is a
 *  plain GPIO (PA4). Wired as hardware NSS the display never responds while
 *  the bus looks entirely correct on a scope. */

/* Power-on sequence, §4.3. */
#define LCD_T2_MEMORY_INIT_MS   1u    /* pixel memory init, >= 1 ms   */
#define LCD_T3_LATCH_RELEASE_US 30u   /* >= 30 us                     */
#define LCD_T4_COM_INIT_US      30u   /* >= 30 us                     */

/* EXTCOMIN toggle rate — continuous, whenever the panel is powered. */
#define LCD_EXTCOMIN_TOGGLE_HZ 1u

void display_wheel_init(void);
void display_wheel_task_1ms(uint32_t now_ms);

/* A watchdog on the one failure that destroys hardware. */
bool display_wheel_extcomin_healthy(uint32_t now_ms);

void display_wheel_clear(void);
void display_wheel_set_pixel(uint8_t x, uint8_t y, lcd_colour_t c);
void display_wheel_flush(void);

/* DISP low shows black while RETAINING the image — the cheapest possible
 * screen blank, and it survives being toggled repeatedly. */
void display_wheel_set_visible(bool on);

/* ---------------------------------------------------------------------------
 * Host-testable
 * ------------------------------------------------------------------------ */

/* Build the two command bytes for a single-line update of `line` (1-based). */
void lcd_line_command(uint8_t line, uint8_t out[2]);

/* Place a 3-bit colour at pixel x within a line buffer. */
void lcd_line_set_pixel(uint8_t *line_buf, uint8_t x, lcd_colour_t c);

/* Read it back — used by the tests to prove packing round-trips. */
lcd_colour_t lcd_line_get_pixel(const uint8_t *line_buf, uint8_t x);

/* Test-only. */
const uint8_t *display_wheel_debug_line(uint8_t y);
bool display_wheel_debug_dirty(uint8_t y);
bool display_wheel_debug_visible(void);

#endif /* BOARD_WHEEL */
#endif /* DISPLAY_WHEEL_H */
