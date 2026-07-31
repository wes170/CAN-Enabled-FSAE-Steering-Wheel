/* display_wheel.h — Sharp LS027B7DH01, 400x240 mono, reflective memory-in-pixel.
 *
 * A memory-in-pixel panel holds its image with no refresh, so there is no frame
 * loop here — write the lines that changed and stop. Ambient light IMPROVES its
 * contrast instead of washing it out, which is why it is on a steering wheel in
 * direct sun.
 *
 * ⚠ THE ONE THING THAT DESTROYS THIS PANEL: EXTCOMIN must keep toggling.
 * The liquid crystal needs its drive polarity inverted periodically; left at one
 * polarity, a DC bias builds across the cell and damages it PERMANENTLY. That is
 * a hardware destruction path, not a picture-quality issue, and it is invisible
 * until it is too late.
 *
 * Rev B.11 replaced the JDI LPM013M126A (176x176, 8 colours) with this panel:
 * landscape aspect, roughly four times the active area, mono instead of colour.
 * The MCU pins and nets did not move -- the 10-pin FPC pinout is identical --
 * but the WIRE FORMAT is a different protocol, so this driver was rewritten
 * rather than retuned.
 *
 * Every constant below is read from Sharp specification LCP-2110015A, issued
 * 2010-06-21, saved at hardware/lib/datasheet-LS027B7DH01-sharp.pdf.
 */
#ifndef DISPLAY_WHEEL_H
#define DISPLAY_WHEEL_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_WHEEL

/* §3 Mechanical Specification: 400 (H) x 240 (V) dots. */
#define LCD_WIDTH   400u
#define LCD_HEIGHT  240u

/* 1 bit per pixel; 400 / 8 = exactly 50 bytes per line, no ragged edge. */
#define LCD_LINE_BYTES (LCD_WIDTH / 8u)

/* 240 * 50 = 12,000 bytes. Comfortable in the G474's 128 kB, but no longer the
 * rounding error the old 176x176 buffer was -- worth knowing before anyone adds
 * a second full-screen buffer for double-buffering. */
#define LCD_FB_BYTES (LCD_HEIGHT * LCD_LINE_BYTES)

/* Gate lines are addressed 1..240. Line 0 does not exist. */
#define LCD_FIRST_LINE 1u

/* Mono. Kept as a named type rather than a bool so call sites read the same as
 * they did with the colour panel, and so a stray integer cannot silently become
 * "white". */
typedef enum {
    LCD_BLACK = 0,
    LCD_WHITE = 1,
} lcd_colour_t;

/*  ═══ WIRE FORMAT (§6-5-1, one line) ═══
 *
 *    [ M0 M1 M2 + 5 dummy ]  [ 8-bit gate address ]  [ 400 data ]  [ 16 dummy ]
 *          8 clocks                 8 clocks           400 clocks    16 clocks
 *
 *    M0  mode flag        H = data update, L = display mode
 *    M1  frame inversion  only meaningful when EXTMODE = L. This board straps
 *                         EXTMODE high and drives COM from EXTCOMIN, so M1
 *                         stays low and the panel ignores it.
 *    M2  all clear        §6-5-4
 *
 *  ⚠ THIS IS NOT THE JDI FORMAT. That part used 6 mode bits and a 10-bit
 *  MSB-first gate address. Reusing that code here gives a panel that responds
 *  and then draws in the wrong places -- the worst kind of wrong, because it
 *  looks like it is nearly working.
 */
#define LCD_M0_DATA_UPDATE 0x80u   /* bit 7 = first on the wire, MSB-first SPI */
#define LCD_M1_FRAME_INV   0x40u
#define LCD_M2_ALL_CLEAR   0x20u

/*  ═══ THE GATE ADDRESS IS LSB-FIRST ═══
 *
 *  Read straight off the §6-6 "Gate Line Address Setup" table rather than
 *  inferred from convention:
 *
 *      L1    AG0=H, rest L                 ->  0x80 on the wire
 *      L2    AG1=H, rest L                 ->  0x40
 *      L3    AG0=H AG1=H, rest L           ->  0xC0
 *      L240  AG0..AG3=L, AG4..AG7=H        ->  0x0F
 *
 *  i.e. the line number 1..240 in binary with AG0 transmitted FIRST. That is
 *  the Sharp bit-reversed convention, and those four rows are the test vectors
 *  in test_display.c -- asserted against the datasheet table, not against this
 *  implementation's own output.
 *
 *  SPI1 stays in its conventional MSB-first mode and the reversal happens here,
 *  in software, where it is visible and host-testable. Setting the peripheral's
 *  LSBFIRST bit would work equally well and would hide the single fact most
 *  likely to be broken by someone reconfiguring SPI later.
 */
uint8_t lcd_reverse8(uint8_t v);

/*  SCLK ceiling. Table 6-3-1: 1.00 MHz typical, 2.00 MHz MAXIMUM. SPI1 hangs
 *  off a 170 MHz bus and will clock far faster if the prescaler is chosen for
 *  convenience rather than against this number. */
#define LCD_SCLK_MAX_HZ LCD_SPI_MAX_HZ   /* board_config.h -- one home */
_Static_assert(LCD_SCLK_MAX_HZ == 2000000u,
               "Table 6-3-1 gives fSCLK max as 2 MHz; board_config.h disagrees");

/*  A full frame is 240 * (8 + 8 + 400 + 16) clocks: 104 ms at 1 MHz, 52 ms at
 *  2 MHz. Redrawing everything every time caps the panel near 10 fps, which is
 *  why this driver tracks dirty lines and why the panel's support for arbitrary
 *  single-line addressing matters. */
#define LCD_LINE_CLOCKS       (8u + 8u + LCD_WIDTH + 16u)
#define LCD_FULL_FRAME_CLOCKS (LCD_HEIGHT * LCD_LINE_CLOCKS)

/*  ⚠ SCS IS ACTIVE HIGH — the opposite of nearly every other SPI peripheral.
 *  It therefore CANNOT be SPI1's hardware NSS, which is active low. It is a
 *  plain GPIO (PA4). Wired as hardware NSS the display never responds while the
 *  bus looks entirely correct on a scope. */

/* Power-on sequence, §6-2. */
#define LCD_T2_MEMORY_INIT_MS   1u    /* pixel memory init,        >= 1 ms  */
#define LCD_T3_LATCH_RELEASE_US 30u   /* TCOM latch release,       >= 30 us */
#define LCD_T4_COM_INIT_US      30u   /* TCOM polarity init,       >= 30 us */
#define LCD_T_SCS_AFTER_DISP_US 30u   /* DISP/EXTCOMIN up before SCS starts */

/*  EXTCOMIN toggle rate — continuous, whenever the panel is powered.
 *  Table 6-3-1 gives fCOM as 0.5 .. 10 Hz.
 *
 *  §6-5-5 adds a requirement the JDI part did not have: "The period of EXTCOMIN
 *  should be constant." A 1 ms task tick that occasionally slips under load is
 *  acceptable for the toggle itself, but if this ever moves to a tighter rate
 *  it belongs on a hardware timer output rather than in software. */
#define LCD_EXTCOMIN_TOGGLE_HZ LCD_EXTCOMIN_HZ   /* board_config.h -- one home */
_Static_assert(LCD_EXTCOMIN_TOGGLE_HZ >= 1u && LCD_EXTCOMIN_TOGGLE_HZ <= 10u,
               "Table 6-3-1 gives fCOM as 0.5..10 Hz");

/*  ⚠ IMAGE STICKING — a firmware requirement, not a nicety, and new with this
 *  panel.
 *
 *  Operating note (3): "A still image should be displayed less than two hours;
 *  if it is necessary to display a still image longer than two hours, display
 *  image data must be refreshed."
 *  Operating note (4): a static-electricity event can drop what is held in
 *  pixel memory, and "data update should be executed frequently."
 *
 *  On a steering wheel, gripped by a driver in a synthetic-fibre suit, the
 *  second one is not hypothetical. So the whole frame is rewritten on a timer
 *  even when nothing has changed. Two minutes is far inside the two-hour limit,
 *  costs ~104 ms of SPI, and bounds how long a static-discharge dropout can
 *  leave a stale or blank region on screen. */
#define LCD_ANTI_STICK_REFRESH_MS 120000u

void display_wheel_init(void);
void display_wheel_task_1ms(uint32_t now_ms);

/* A watchdog on the one failure that destroys hardware. */
bool display_wheel_extcomin_healthy(uint32_t now_ms);

void display_wheel_clear(void);
void display_wheel_set_pixel(uint16_t x, uint16_t y, lcd_colour_t c);
void display_wheel_flush(void);

/* DISP low shows all-white while RETAINING the image (§4 Remark 4-2) — the
 * cheapest possible screen blank, and it survives being toggled repeatedly. */
void display_wheel_set_visible(bool on);

/* ---------------------------------------------------------------------------
 * Host-testable
 * ------------------------------------------------------------------------ */

/* Build the two header bytes for a single-line update of `line` (1-based). */
void lcd_line_command(uint8_t line, uint8_t out[2]);

/* Build the two header bytes for all-clear mode (§6-5-4: M0=L, M2=H). */
void lcd_all_clear_command(uint8_t out[2]);

/* Pixel x within a line buffer. x is 0-based here; the datasheet's P1 is x=0. */
void lcd_line_set_pixel(uint8_t *line_buf, uint16_t x, lcd_colour_t c);
lcd_colour_t lcd_line_get_pixel(const uint8_t *line_buf, uint16_t x);

/* Test-only. */
const uint8_t *display_wheel_debug_line(uint16_t y);
bool display_wheel_debug_dirty(uint16_t y);
bool display_wheel_debug_visible(void);

#endif /* BOARD_WHEEL */
#endif /* DISPLAY_WHEEL_H */
