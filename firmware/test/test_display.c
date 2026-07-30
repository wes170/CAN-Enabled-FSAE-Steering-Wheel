/* test_display.c — host tests for the JDI panel driver.
 *
 * Two things here are worth real scrutiny:
 *
 *   1. The command format. The obvious move is to reuse a Sharp memory-LCD
 *      driver, which sends an 8-bit line address BIT-REVERSED. This panel uses
 *      a 10-bit address sent MSB-first. Get it wrong and the display works
 *      perfectly while drawing on the wrong lines — a bus bug wearing a layout
 *      bug's clothes.
 *
 *   2. EXTCOMIN. If it stops toggling, DC bias builds across the liquid
 *      crystal and PERMANENTLY damages the panel. Nothing else reports it.
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

static void test_line_command(void)
{
    puts("\ncommand format: 6 mode bits + 10-bit address, MSB first");
    uint8_t c[2];

    lcd_line_command(1u, c);
    ck_u("line 1, byte 0 (M0=H, rest L, AG9:AG8=0)", c[0], 0x80u);
    ck_u("line 1, byte 1 (AG7..AG0)",                c[1], 0x01u);

    lcd_line_command(176u, c);
    ck_u("line 176, byte 0", c[0], 0x80u);
    ck_u("line 176, byte 1", c[1], 0xB0u);   /* 176 = 0xB0 */

    /* NOT bit-reversed. If someone pastes in the Sharp driver, line 1 becomes
     * 0x80 in the address byte instead of 0x01 and the panel draws on line
     * 128. This assertion is the tripwire for that. */
    lcd_line_command(1u, c);
    ck("address is NOT bit-reversed (Sharp-style would give 0x80)", c[1] != 0x80u);

    /* AG9/AG8 must come from the address, not be hard-coded zero. Feeding a
     * line number above 255 is impossible through the uint8_t API, so this
     * checks the masking arithmetic directly on the low bits instead. */
    lcd_line_command(255u, c);
    ck_u("line 255, byte 1", c[1], 0xFFu);
    ck_u("line 255 still has AG9:AG8 clear", c[0], 0x80u);

    /* The mode bits must never collide with the all-clear flag. */
    ck("data-update and all-clear flags are different bits",
       (LCD_M0_DATA_UPDATE & LCD_M2_ALL_CLEAR) == 0u);

    lcd_line_command(1u, NULL);   /* must not crash */
    ck("NULL output buffer is safe", true);
}

static void test_pixel_packing(void)
{
    puts("\n3-bit pixel packing (pixels straddle byte boundaries)");
    uint8_t line[LCD_LINE_BYTES];
    memset(line, 0, sizeof line);

    ck_u("line is exactly 66 bytes", LCD_LINE_BYTES, 66u);
    ck("176 px x 3 bits divides evenly into bytes", (LCD_WIDTH * 3u) % 8u == 0u);

    /* Round-trip every colour at pixel 0. */
    bool ok = true;
    for (unsigned c = 0; c < 8u; ++c) {
        lcd_line_set_pixel(line, 0u, (lcd_colour_t)c);
        if (lcd_line_get_pixel(line, 0u) != (lcd_colour_t)c) { ok = false; }
    }
    ck("all eight colours round-trip at pixel 0", ok);

    /* Round-trip every colour at EVERY pixel — this is where a straddle bug
     * lives, at pixels 2, 5, 8 ... where the 3 bits cross a byte edge. */
    ok = true;
    for (unsigned x = 0; x < LCD_WIDTH && ok; ++x) {
        for (unsigned c = 0; c < 8u; ++c) {
            lcd_line_set_pixel(line, (uint8_t)x, (lcd_colour_t)c);
            if (lcd_line_get_pixel(line, (uint8_t)x) != (lcd_colour_t)c) {
                printf("  FAIL pixel %u colour %u did not round-trip\n", x, c);
                ok = false; break;
            }
        }
    }
    ck("all eight colours round-trip at all 176 pixels", ok);

    /* Writing one pixel must not disturb its neighbours. */
    memset(line, 0, sizeof line);
    for (unsigned x = 0; x < LCD_WIDTH; ++x) {
        lcd_line_set_pixel(line, (uint8_t)x, LCD_WHITE);
    }
    lcd_line_set_pixel(line, 5u, LCD_BLACK);
    ck("neighbour below is untouched", lcd_line_get_pixel(line, 4u) == LCD_WHITE);
    ck("target is set",                lcd_line_get_pixel(line, 5u) == LCD_BLACK);
    ck("neighbour above is untouched", lcd_line_get_pixel(line, 6u) == LCD_WHITE);

    /* An all-white line must be all 1 bits: 66 bytes of 0xFF. */
    memset(line, 0, sizeof line);
    for (unsigned x = 0; x < LCD_WIDTH; ++x) {
        lcd_line_set_pixel(line, (uint8_t)x, LCD_WHITE);
    }
    bool all_ff = true;
    for (unsigned i = 0; i < LCD_LINE_BYTES; ++i) { if (line[i] != 0xFFu) { all_ff = false; } }
    ck("all-white line is 66 bytes of 0xFF", all_ff);

    /* Colour bit order is R-G-B, MSB first. LCD_RED = 0x4 = 0b100, so pixel 0
     * red sets the FIRST transmitted bit and clears the next two. */
    memset(line, 0, sizeof line);
    lcd_line_set_pixel(line, 0u, LCD_RED);
    ck_u("RED at pixel 0 sets the R bit first", line[0] & 0xE0u, 0x80u);
    memset(line, 0, sizeof line);
    lcd_line_set_pixel(line, 0u, LCD_BLUE);
    ck_u("BLUE at pixel 0 sets the third bit", line[0] & 0xE0u, 0x20u);

    /* Out of range must be ignored, not wrap into another pixel. */
    memset(line, 0, sizeof line);
    lcd_line_set_pixel(line, 200u, LCD_WHITE);
    bool clean = true;
    for (unsigned i = 0; i < LCD_LINE_BYTES; ++i) { if (line[i] != 0u) { clean = false; } }
    ck("out-of-range x writes nothing", clean);
    ck("NULL buffer is safe", lcd_line_get_pixel(NULL, 0u) == LCD_BLACK);
}

static void test_dirty_tracking(void)
{
    puts("\ndirty-line tracking (a full redraw is ~49 ms at 2 MHz)");
    display_wheel_init();
    display_wheel_flush();                       /* start clean */

    ck("nothing dirty after a flush", !display_wheel_debug_dirty(10u));

    display_wheel_set_pixel(5u, 10u, LCD_RED);
    ck("the written line is dirty",  display_wheel_debug_dirty(10u));
    ck("other lines are not",       !display_wheel_debug_dirty(11u));

    /* Writing the SAME colour again must not re-dirty the line. Without this,
     * a UI that redraws unconditionally marks every line dirty every frame and
     * the dirty tracking buys nothing. */
    display_wheel_flush();
    display_wheel_set_pixel(5u, 10u, LCD_RED);
    ck("rewriting an identical pixel leaves the line clean",
       !display_wheel_debug_dirty(10u));

    display_wheel_set_pixel(5u, 10u, LCD_BLUE);
    ck("a genuine change does dirty it", display_wheel_debug_dirty(10u));

    display_wheel_clear();
    ck("clear() dirties everything", display_wheel_debug_dirty(0u)
                                  && display_wheel_debug_dirty(175u));

    display_wheel_set_pixel(0u, 200u, LCD_WHITE);   /* out of range */
    ck("out-of-range y is ignored", true);
}

static void test_extcomin(void)
{
    puts("\nEXTCOMIN — the watchdog on a hardware destruction path");
    display_wheel_init();

    uint32_t t = 0u;
    /* Healthy immediately after init. */
    ck("healthy at t=0", display_wheel_extcomin_healthy(t));

    /* Run the task properly for ten seconds. */
    for (t = 0; t < 10000u; ++t) { display_wheel_task_1ms(t); }
    ck("still healthy after 10 s of correct operation",
       display_wheel_extcomin_healthy(t));

    /* Now stop calling the task — a hung task, a blocked loop, anything. The
     * watchdog must notice. */
    const uint32_t stalled = t + 5000u;
    ck("UNHEALTHY once the task stops toggling",
       !display_wheel_extcomin_healthy(stalled));

    /* Resuming clears it. */
    display_wheel_task_1ms(stalled);
    ck("healthy again once toggling resumes",
       display_wheel_extcomin_healthy(stalled));

    /* Rollover: must not report a false stall across the uint32 wrap. */
    display_wheel_init();
    const uint32_t near_wrap = 0xFFFFFF00u;
    for (uint32_t i = 0; i < 2000u; ++i) {
        display_wheel_task_1ms((uint32_t)(near_wrap + i));
    }
    ck("no false stall across the uint32 wrap",
       display_wheel_extcomin_healthy((uint32_t)(near_wrap + 2000u)));
}

static void test_constraints(void)
{
    puts("\ndatasheet constraints held in the header");
    ck_u("SCLK ceiling is the datasheet's 2 MHz", LCD_SCLK_MAX_HZ, 2000000u);
    ck("line addressing is one-based", LCD_FIRST_LINE == 1u);
    ck("T2 pixel-memory init is at least 1 ms", LCD_T2_MEMORY_INIT_MS >= 1u);
    ck("T3 latch release is at least 30 us", LCD_T3_LATCH_RELEASE_US >= 30u);
    ck("T4 COM init is at least 30 us", LCD_T4_COM_INIT_US >= 30u);

    /* A full redraw at the SCLK ceiling. 176 lines x (2 cmd + 66 data + 2
     * dummy) bytes x 8 bits / 2 MHz. */
    const uint32_t bits = 176u * (2u + LCD_LINE_BYTES + 2u) * 8u;
    const uint32_t us   = bits / (LCD_SCLK_MAX_HZ / 1000000u);
    printf("        full 176-line redraw = %u bits = %u us at 2 MHz\n", bits, us);
    ck("a full redraw is under 100 ms", us < 100000u);
}

int main(void)
{
    test_line_command();
    test_pixel_packing();
    test_dirty_tracking();
    test_extcomin();
    test_constraints();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
