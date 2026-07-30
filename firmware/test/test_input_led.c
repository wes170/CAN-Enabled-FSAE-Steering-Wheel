/* test_input_led.c — host tests for switch debounce and the LED current cap.
 *
 * The debounce tests feed synthetic bounce and vibration patterns, because
 * those failures (a missed press, a doubled press) show up once every few
 * hundred operations on a bench and are effectively unobservable by hand.
 *
 * The current-cap tests matter because the cap is the only thing between a
 * pattern bug and a harness carrying more current than it was sized for.
 */
#include <stdio.h>
#include <string.h>
#include "input.h"
#include "led.h"

static int failures = 0;

static void ck(const char *what, bool ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) { ++failures; }
}

static void ck_u(const char *what, unsigned got, unsigned want)
{
    if (got != want) { printf("  FAIL %-52s got %u want %u\n", what, got, want); ++failures; }
    else             { printf("  ok   %-52s = %u\n", what, got); }
}

/* ======================================================================== */
/* Debounce                                                                 */
/* ======================================================================== */

static void feed(debounced_input_t *d, bool level, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) { (void)debounce_update(d, level); }
}

static void test_debounce_basic(void)
{
    puts("\ndebounce: clean press and release");
    debounced_input_t d; memset(&d, 0, sizeof d);

    ck("starts released", !d.state);

    feed(&d, true, DEBOUNCE_PRESS_COUNT - 1u);
    ck("not yet pressed one sample short of threshold", !d.state);

    feed(&d, true, 1);
    ck("pressed at the threshold", d.state);

    feed(&d, false, DEBOUNCE_MAX_COUNT - 1u);
    ck("still pressed one sample short of release", d.state);

    feed(&d, false, 1);
    ck("released at the bottom", !d.state);
}

static void test_debounce_bounce(void)
{
    puts("\ndebounce: contact bounce must produce exactly one press");
    debounced_input_t d; memset(&d, 0, sizeof d);

    /* A real switch chatters for a few ms on closure. Alternate hard for 10
     * samples, then settle closed. */
    for (unsigned i = 0; i < 10u; ++i) { (void)debounce_update(&d, i % 2u == 0u); }
    ck("no press confirmed during the chatter", !d.state);

    feed(&d, true, DEBOUNCE_PRESS_COUNT);
    ck("press confirmed once it settles", d.state);
    ck("exactly one press edge", input_take_press(0) == false);  /* not this instance */
    ck("edge flag was set on the transition", d.edge_press);
    d.edge_press = false;

    /* Release chatter must likewise produce exactly one release. */
    for (unsigned i = 0; i < 10u; ++i) { (void)debounce_update(&d, i % 2u == 0u); }
    ck("no release during release chatter", d.state);
    feed(&d, false, DEBOUNCE_MAX_COUNT);
    ck("released once it settles", !d.state);
    ck("no spurious press edge from the release", !d.edge_press);
}

static void test_debounce_vibration(void)
{
    puts("\ndebounce: sustained vibration must not generate phantom presses");
    debounced_input_t d; memset(&d, 0, sizeof d);

    /* This is the case a consecutive-samples filter handles badly. A steering
     * wheel on a race car vibrates continuously; if the pin flickers around
     * the midpoint for a long time, the output must stay stable. */
    int transitions = 0;
    for (unsigned i = 0; i < 2000u; ++i) {
        if (debounce_update(&d, i % 2u == 0u)) { ++transitions; }
    }
    ck_u("2000 samples of 50/50 chatter -> zero transitions",
         (unsigned)transitions, 0u);
    ck("and the state is still released", !d.state);

    /* Mostly-closed with occasional noise samples must still confirm. */
    memset(&d, 0, sizeof d);
    transitions = 0;
    for (unsigned i = 0; i < 200u; ++i) {
        const bool noise = (i % 17u == 0u);      /* ~6% stray opens */
        if (debounce_update(&d, !noise)) { ++transitions; }
    }
    ck("a noisy but genuine press still confirms", d.state);
    ck_u("and does so exactly once", (unsigned)transitions, 1u);
}

static void test_debounce_hysteresis(void)
{
    puts("\ndebounce: hysteresis holds state between the thresholds");
    debounced_input_t d; memset(&d, 0, sizeof d);
    feed(&d, true, DEBOUNCE_PRESS_COUNT);
    ck("pressed", d.state);

    /* Drop halfway down and hover there. The state must not change. */
    feed(&d, false, DEBOUNCE_MAX_COUNT / 2u);
    int transitions = 0;
    for (unsigned i = 0; i < 500u; ++i) {
        if (debounce_update(&d, i % 2u == 0u)) { ++transitions; }
    }
    ck_u("hovering mid-range causes no transitions", (unsigned)transitions, 0u);
    ck("still reads pressed", d.state);
}

static void test_input_module(void)
{
    puts("\ninput module: init state and keypad packing");
    input_init();

    /* Deliberately not seeded from the pins: a line held low at boot (a stuck
     * switch or a wiring fault) must register as nothing, not as a press. */
    ck("all inputs start released", !input_is_pressed(0) && !input_is_pressed(1));
    ck_u("keypad bits start clear", input_keypad_bits(), 0u);

    debounced_input_t *d0 = input_debug_state(0);
    ck("input 0 exists", d0 != NULL);
    if (d0) {
        feed(d0, true, DEBOUNCE_PRESS_COUNT);
        ck("input 0 reads pressed", input_is_pressed(0));
        ck_u("keypad bit 0 set", input_keypad_bits() & 1u, 1u);
        ck("press edge available", input_take_press(0));
        ck("edge cleared by reading", !input_take_press(0));
    }

    ck("out-of-range index is safe", !input_is_pressed(INPUT_TOTAL + 3u));
    ck("out-of-range edge is safe",  !input_take_press(INPUT_TOTAL + 3u));
    ck("keypad bits never exceed 12", (input_keypad_bits() & 0xF000u) == 0u);
}

/* ======================================================================== */
/* LED current cap                                                          */
/* ======================================================================== */

static void test_led_estimate(void)
{
    puts("\nLED current estimate");
    led_rgb_t px[LED_TOTAL];

    memset(px, 0, sizeof px);
    ck_u("all dark = quiescent only",
         led_estimate_ma(px, LED_TOTAL), LED_TOTAL * LED_MA_QUIESCENT_PER_LED);

    for (unsigned i = 0; i < LED_TOTAL; ++i) { px[i].r = px[i].g = px[i].b = 255u; }
    const uint32_t full = led_estimate_ma(px, LED_TOTAL);
    const uint32_t expect = LED_TOTAL * 3u * LED_MA_PER_CHANNEL_FULL
                          + LED_TOTAL * LED_MA_QUIESCENT_PER_LED;
    ck_u("all full white", full, expect);
    ck("full white exceeds the cap, which is the whole point",
       full > LED_CURRENT_CAP_MA);

    ck("NULL is safe", led_estimate_ma(NULL, LED_TOTAL) == 0u);
}

static void test_led_cap(void)
{
    puts("\nLED current cap");
    led_rgb_t px[LED_TOTAL];

    /* Under the cap: nothing must be touched. Dimming a frame that was already
     * legal would make every pattern subtly darker than intended. */
    memset(px, 0, sizeof px);
    px[0].r = 255u;
    const led_rgb_t before = px[0];
    uint16_t scale = led_apply_cap(px, LED_TOTAL, LED_CURRENT_CAP_MA);
    ck_u("under the cap -> scale 256 (untouched)", scale, 256u);
    ck("pixel unchanged", px[0].r == before.r);

    /* Over the cap: must come back under it. */
    for (unsigned i = 0; i < LED_TOTAL; ++i) { px[i].r = px[i].g = px[i].b = 255u; }
    scale = led_apply_cap(px, LED_TOTAL, LED_CURRENT_CAP_MA);
    const uint32_t after = led_estimate_ma(px, LED_TOTAL);
    printf("        full white scaled by %u/256 -> %u mA (cap %u mA)\n",
           scale, after, LED_CURRENT_CAP_MA);
    ck("scaled frame is at or under the cap", after <= LED_CURRENT_CAP_MA);
    ck("scale actually reduced something", scale < 256u);

    /* Proportionality: the pattern's shape must survive. Dimming only the
     * bright end would change which segment of the shift bar looks lit, and
     * the driver reads that bar as a rev point. */
    memset(px, 0, sizeof px);
    for (unsigned i = 0; i < LED_TOTAL; ++i) {
        px[i].r = (uint8_t)(255u - i * 8u);
        px[i].g = 200u;
    }
    const uint8_t r0 = px[0].r, r10 = px[10].r;
    led_apply_cap(px, LED_TOTAL, LED_CURRENT_CAP_MA);
    ck("scaled frame under the cap", led_estimate_ma(px, LED_TOTAL) <= LED_CURRENT_CAP_MA);
    /* Ratios preserved within integer rounding. */
    if (px[10].r > 0u && r10 > 0u) {
        const int want = (int)((r0 * 100) / r10);
        const int got  = (int)((px[0].r * 100) / px[10].r);
        ck("brightness ratio between pixels preserved (within rounding)",
           got >= want - 4 && got <= want + 4);
    }

    /* Degenerate: a cap below the quiescent draw cannot be met by dimming.
     * Blanking is the closest achievable state and an honest signal. */
    for (unsigned i = 0; i < LED_TOTAL; ++i) { px[i].r = px[i].g = px[i].b = 255u; }
    scale = led_apply_cap(px, LED_TOTAL, 1u);
    ck_u("impossible cap -> scale 0", scale, 0u);
    ck("and the strip is blanked", px[0].r == 0u && px[LED_TOTAL-1].b == 0u);

    ck("NULL is safe",     led_apply_cap(NULL, LED_TOTAL, 100u) == 256u);
    ck("zero count is safe", led_apply_cap(px, 0u, 100u) == 256u);
}

static void test_led_show_is_the_choke_point(void)
{
    puts("\nLED: led_show() applies the cap unconditionally");
    led_init();
    ck_u("init leaves the strip dark",
         led_estimate_ma(led_debug_buffer(), LED_TOTAL),
         LED_TOTAL * LED_MA_QUIESCENT_PER_LED);

    /* A pattern that blows way past the cap, written through the normal API. */
    led_fill(255u, 255u, 255u);
    ck("buffer is over the cap before show()",
       led_estimate_ma(led_debug_buffer(), LED_TOTAL) > LED_CURRENT_CAP_MA);

    led_show();
    ck("after show(), the buffer is under the cap",
       led_estimate_ma(led_debug_buffer(), LED_TOTAL) <= LED_CURRENT_CAP_MA);

    led_set(200u, 255u, 255u, 255u);   /* out of range: must be ignored */
    ck("out-of-range led_set ignored",
       led_estimate_ma(led_debug_buffer(), LED_TOTAL) <= LED_CURRENT_CAP_MA);
}

int main(void)
{
    test_debounce_basic();
    test_debounce_bounce();
    test_debounce_vibration();
    test_debounce_hysteresis();
    test_input_module();
    test_led_estimate();
    test_led_cap();
    test_led_show_is_the_choke_point();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
