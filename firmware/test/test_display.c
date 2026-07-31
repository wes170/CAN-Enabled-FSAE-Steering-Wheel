/* test_display.c — host tests for the Sharp LS027B7DH01 panel driver.
 *
 * Three things here are worth real scrutiny:
 *
 *   1. The gate address is LSB-FIRST. The vectors below are transcribed from
 *      the DATASHEET's own §6-6 "Gate Line Address Setup" table, not generated
 *      from this implementation — so they fail if the reversal is dropped, and
 *      they would have failed the previous JDI driver, which sent a 10-bit
 *      MSB-first address. Get this wrong and the panel works perfectly while
 *      drawing on the wrong lines: a bus bug wearing a layout bug's clothes.
 *
 *   2. EXTCOMIN. If it stops toggling, DC bias builds across the liquid
 *      crystal and PERMANENTLY damages the panel. Nothing else reports it.
 *
 *   3. The anti-sticking refresh. New with this panel: a still image must not
 *      stand more than two hours, and a static-discharge event can drop pixel
 *      memory outright. If the periodic full rewrite stops, nothing complains
 *      until someone notices a stale corner of the screen.
 */
#include <stdio.h>
#include <string.h>
#include "display_wheel.h"

static int failures = 0;

static void ck(const char *what, bool ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) { ++failures; }
}

static void ck_u(const char *what, unsigned got, unsigned want)
{
    if (got != want) { printf("  FAIL %-50s got 0x%02X want 0x%02X\n", what, got, want); ++failures; }
    else             { printf("  ok   %-50s = 0x%02X\n", what, got); }
}

/* ---------------------------------------------------------------------------
 * 1. Gate address — asserted against the datasheet table, not against us
 * ------------------------------------------------------------------------ */

static void test_line_command(void)
{
    puts("\ngate address: 8-bit, LSB-first (vectors from spec section 6-6 table)");
    uint8_t c[2];

    /* Each row is read off the datasheet's table of AG0..AG7 levels and
     * assembled MSB-first for the wire, since AG0 is transmitted first:
     *
     *   L1    H L L L L L L L  -> 0b10000000 = 0x80
     *   L2    L H L L L L L L  -> 0b01000000 = 0x40
     *   L3    H H L L L L L L  -> 0b11000000 = 0xC0
     *   L238  L H H H L H H H  -> 0b01110111 = 0x77
     *   L239  H H H H L H H H  -> 0b11110111 = 0xF7
     *   L240  L L L L H H H H  -> 0b00001111 = 0x0F
     */
    static const struct { unsigned line; unsigned want; } VEC[] = {
        {   1u, 0x80u }, {   2u, 0x40u }, {   3u, 0xC0u },
        { 238u, 0x77u }, { 239u, 0xF7u }, { 240u, 0x0Fu },
    };

    for (unsigned i = 0; i < sizeof VEC / sizeof VEC[0]; ++i) {
        char label[72];
        snprintf(label, sizeof label, "line %u address byte (datasheet 6-6)", VEC[i].line);
        lcd_line_command((uint8_t)VEC[i].line, c);
        ck_u(label, c[1], VEC[i].want);
    }
    lcd_line_command(1u, c);
    ck_u("mode byte is M0 only", c[0], 0x80u);

    /* The tripwire that catches pasting the old JDI code back in: that driver
     * sent the line number straight through, so line 1 would give 0x01 here
     * and the panel would draw on line 128. */
    ck("address IS bit-reversed (JDI-style would give 0x01)", c[1] == 0x80u);

    /* M1 must stay low: frame inversion is meaningful only when EXTMODE = L,
     * and this board straps EXTMODE high. */
    ck("M1 (frame inversion) is not set - EXTMODE is strapped high",
       (c[0] & LCD_M1_FRAME_INV) == 0u);

    bool rev_ok = true;
    for (unsigned v = 0; v < 256u; ++v) {
        if (lcd_reverse8(lcd_reverse8((uint8_t)v)) != (uint8_t)v) { rev_ok = false; break; }
    }
    ck("lcd_reverse8 is its own inverse across all 256 values", rev_ok);

    /* All-clear mode, section 6-5-4: M0 = L, M2 = H. */
    lcd_all_clear_command(c);
    ck_u("all-clear byte 0 (M0=L, M2=H)", c[0], 0x20u);
    ck_u("all-clear byte 1 (dummy)",      c[1], 0x00u);
    ck("all-clear does NOT set M0 - that would make it a data write",
       (c[0] & LCD_M0_DATA_UPDATE) == 0u);
    ck("data-update and all-clear flags are different bits",
       (LCD_M0_DATA_UPDATE & LCD_M2_ALL_CLEAR) == 0u);

    lcd_line_command(1u, NULL);        /* must not crash */
    lcd_all_clear_command(NULL);
    ck("NULL output buffer is safe", true);
}

/* ---------------------------------------------------------------------------
 * 2. Pixel packing — 1 bit, and 400 divides evenly for once
 * ------------------------------------------------------------------------ */

static void test_pixel_packing(void)
{
    puts("\n1-bit pixel packing");
    uint8_t line[LCD_LINE_BYTES];
    memset(line, 0, sizeof line);

    ck("400 px x 1 bit divides evenly into bytes", (LCD_WIDTH % 8u) == 0u);
    ck_u("bytes per line", LCD_LINE_BYTES, 50u);
    ck_u("frame buffer bytes", LCD_FB_BYTES, 12000u);

    /* Pixel 0 is the MSB of byte 0 - it goes first on the wire. */
    lcd_line_set_pixel(line, 0u, LCD_WHITE);
    ck_u("pixel 0 white sets bit 7 of byte 0", line[0], 0x80u);
    lcd_line_set_pixel(line, 7u, LCD_WHITE);
    ck_u("pixel 7 white sets bit 0 of byte 0", line[0], 0x81u);
    lcd_line_set_pixel(line, 8u, LCD_WHITE);
    ck_u("pixel 8 lands in byte 1", line[1], 0x80u);
    lcd_line_set_pixel(line, 0u, LCD_BLACK);
    ck_u("clearing pixel 0 leaves pixel 7 alone", line[0], 0x01u);

    bool ok = true;
    for (uint16_t x = 0; x < LCD_WIDTH && ok; ++x) {
        lcd_line_set_pixel(line, x, LCD_WHITE);
        if (lcd_line_get_pixel(line, x) != LCD_WHITE) { ok = false; }
        lcd_line_set_pixel(line, x, LCD_BLACK);
        if (lcd_line_get_pixel(line, x) != LCD_BLACK) { ok = false; }
    }
    ck("every one of 400 pixels round-trips both ways", ok);

    memset(line, 0x00, sizeof line);
    lcd_line_set_pixel(line, LCD_WIDTH, LCD_WHITE);
    lcd_line_set_pixel(line, (uint16_t)(LCD_WIDTH + 500u), LCD_WHITE);
    bool clean = true;
    for (unsigned i = 0; i < LCD_LINE_BYTES; ++i) { if (line[i]) { clean = false; } }
    ck("x >= 400 is ignored rather than wrapping into the next line", clean);
    ck("reading past the end returns black", lcd_line_get_pixel(line, LCD_WIDTH) == LCD_BLACK);
    ck("NULL line buffer is safe", lcd_line_get_pixel(NULL, 0u) == LCD_BLACK);
}

/* ---------------------------------------------------------------------------
 * 3. Dirty tracking — 240 lines, and the widened coordinate types
 * ------------------------------------------------------------------------ */

static void test_dirty_tracking(void)
{
    puts("\ndirty-line tracking");
    display_wheel_init();
    display_wheel_flush();                       /* start clean */

    ck("nothing dirty after a flush", !display_wheel_debug_dirty(0u));

    display_wheel_set_pixel(10u, 5u, LCD_WHITE);
    ck("writing a pixel dirties its line", display_wheel_debug_dirty(5u));
    ck("and only its line", !display_wheel_debug_dirty(6u));

    /* Rewriting the same value must NOT re-dirty - this is what keeps a redraw
     * of unchanged content free. */
    display_wheel_flush();
    display_wheel_set_pixel(10u, 5u, LCD_WHITE);
    ck("rewriting an identical pixel leaves the line clean",
       !display_wheel_debug_dirty(5u));

    /* The far corner. Neither 399 nor 239 fits the uint8_t coordinates the JDI
     * driver used - this is the regression guard for the widened types. */
    display_wheel_set_pixel(399u, 239u, LCD_WHITE);
    ck("pixel (399, 239) reaches the last line", display_wheel_debug_dirty(239u));
    ck("and reads back", lcd_line_get_pixel(display_wheel_debug_line(239u), 399u) == LCD_WHITE);

    display_wheel_clear();
    ck("clear() marks every line dirty (0)",   display_wheel_debug_dirty(0u));
    ck("clear() marks every line dirty (239)", display_wheel_debug_dirty(239u));
    ck("out-of-range line is not dirty", !display_wheel_debug_dirty(240u));
    ck("out-of-range line has no buffer", display_wheel_debug_line(240u) == NULL);
}

/* ---------------------------------------------------------------------------
 * 4. EXTCOMIN — the destruction watchdog
 * ------------------------------------------------------------------------ */

static void test_extcomin(void)
{
    puts("\nEXTCOMIN watchdog (the failure that destroys the panel)");
    display_wheel_init();

    for (uint32_t t = 0; t <= 2000u; ++t) { display_wheel_task_1ms(t); }
    ck("healthy while the task runs", display_wheel_extcomin_healthy(2000u));

    ck("stall is detected after three half-periods",
       !display_wheel_extcomin_healthy(2000u + 1600u));

    const uint32_t near_wrap = 0xFFFFFF00u;
    display_wheel_init();
    for (uint32_t i = 0; i < 2000u; ++i) { display_wheel_task_1ms(near_wrap + i); }
    ck("no false stall across the uint32 wrap",
       display_wheel_extcomin_healthy((uint32_t)(near_wrap + 2000u)));
}

/* ---------------------------------------------------------------------------
 * 5. Anti-sticking refresh — new requirement with this panel
 * ------------------------------------------------------------------------ */

static void test_anti_stick_refresh(void)
{
    puts("\nanti-sticking periodic full rewrite");
    display_wheel_init();
    display_wheel_flush();                       /* clean slate */
    ck("clean immediately after a flush", !display_wheel_debug_dirty(100u));

    for (uint32_t t = 0; t < 1000u; ++t) { display_wheel_task_1ms(t); }
    ck("no spurious refresh 1 s in", !display_wheel_debug_dirty(100u));

    /* Cross the interval. Step in 1 s jumps rather than 1 ms so the test does
     * not run 120,000 iterations; the task only compares timestamps. */
    for (uint32_t t = 1000u; t <= LCD_ANTI_STICK_REFRESH_MS + 1000u; t += 1000u) {
        display_wheel_task_1ms(t);
    }
    ck("every line is dirty after the refresh interval",
       display_wheel_debug_dirty(0u) && display_wheel_debug_dirty(239u));

    ck("the interval is far inside the datasheet's two-hour limit",
       LCD_ANTI_STICK_REFRESH_MS < (2u * 60u * 60u * 1000u));
    ck("and long enough not to hog SPI (>= 10 s)",
       LCD_ANTI_STICK_REFRESH_MS >= 10000u);
}

/* ---------------------------------------------------------------------------
 * 6. Datasheet constraints held in the header
 * ------------------------------------------------------------------------ */

static void test_constraints(void)
{
    puts("\ndatasheet constraints held in the header");
    ck_u("SCLK ceiling is the datasheet's 2 MHz", LCD_SCLK_MAX_HZ, 2000000u);
    ck("line addressing is one-based", LCD_FIRST_LINE == 1u);
    ck("T2 pixel-memory init is at least 1 ms", LCD_T2_MEMORY_INIT_MS >= 1u);
    ck("T3 latch release is at least 30 us", LCD_T3_LATCH_RELEASE_US >= 30u);
    ck("T4 COM init is at least 30 us", LCD_T4_COM_INIT_US >= 30u);
    ck("SCS waits >= 30 us after DISP/EXTCOMIN (6-2 note 1)",
       LCD_T_SCS_AFTER_DISP_US >= 30u);
    ck("EXTCOMIN rate is inside fCOM 0.5..10 Hz",
       LCD_EXTCOMIN_TOGGLE_HZ >= 1u && LCD_EXTCOMIN_TOGGLE_HZ <= 10u);

    /* Clocks per line straight from 6-5-1: 8 mode + 8 address + 400 data +
     * 16 transfer. */
    ck_u("clocks per line (6-5-1)", LCD_LINE_CLOCKS, 432u);
    ck_u("clocks per full frame",   LCD_FULL_FRAME_CLOCKS, 103680u);

    const uint32_t us_at_1mhz = LCD_FULL_FRAME_CLOCKS;
    const uint32_t us_at_2mhz = LCD_FULL_FRAME_CLOCKS / 2u;
    printf("        full 240-line redraw = %u clocks = %u us at 1 MHz, %u us at 2 MHz\n",
           LCD_FULL_FRAME_CLOCKS, us_at_1mhz, us_at_2mhz);
    ck("a full redraw at 1 MHz exceeds 100 ms - dirty-line tracking is REQUIRED",
       us_at_1mhz > 100000u);
    ck("even at the 2 MHz ceiling it exceeds 50 ms", us_at_2mhz > 50000u);
}

int main(void)
{
    test_line_command();
    test_pixel_packing();
    test_dirty_tracking();
    test_extcomin();
    test_anti_stick_refresh();
    test_constraints();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
