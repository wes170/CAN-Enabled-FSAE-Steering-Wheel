/* test_encoder.c — host tests for the encoder position logic.
 *
 * The two things worth being certain about:
 *   1. Counter wrap. The timer is free-running, so turning a knob down through
 *      zero reads 0x0002 -> 0xFFFE. Handled wrongly that is +65532 counts and
 *      the knob leaps a full turn.
 *   2. End-stops. These are trim knobs; the ECU reads their position as a fuel
 *      or boost adjustment. Wrapping from maximum to zero would command a jump
 *      between the extremes of the trim range on a running engine.
 */
#include <stdio.h>
#include <string.h>
#include "encoder.h"
#include "haltech_can.h"

static int failures = 0;

static void ck(const char *what, bool ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) { ++failures; }
}

static void ck_i(const char *what, int got, int want)
{
    if (got != want) { printf("  FAIL %-54s got %d want %d\n", what, got, want); ++failures; }
    else             { printf("  ok   %-54s = %d\n", what, got); }
}

static encoder_t mk(uint8_t detents, uint8_t start)
{
    encoder_t e;
    memset(&e, 0, sizeof e);
    e.detents  = detents;
    e.position = start;
    return e;
}

/* Turn a knob by whole detents. */
static void turn(encoder_t *e, int detents)
{
    const int step = detents > 0 ? ENC_COUNTS_PER_DETENT : -ENC_COUNTS_PER_DETENT;
    for (int i = 0; i < (detents > 0 ? detents : -detents); ++i) {
        (void)encoder_apply_delta(e, (int16_t)step);
    }
}

static void test_raw_delta_wrap(void)
{
    puts("\nraw counter delta (16-bit free-running, must survive wrap)");
    ck_i("no movement",              encoder_raw_delta(1000u, 1000u), 0);
    ck_i("up 4",                     encoder_raw_delta(1000u, 1004u), 4);
    ck_i("down 4",                   encoder_raw_delta(1004u, 1000u), -4);
    /* The cases that break a naive implementation. */
    ck_i("up across the wrap",       encoder_raw_delta(0xFFFEu, 0x0002u), 4);
    ck_i("down across the wrap",     encoder_raw_delta(0x0002u, 0xFFFEu), -4);
    ck_i("up from 0xFFFF to 0",      encoder_raw_delta(0xFFFFu, 0x0000u), 1);
    ck_i("down from 0 to 0xFFFF",    encoder_raw_delta(0x0000u, 0xFFFFu), -1);
}

static void test_counts_per_detent(void)
{
    puts("\ncounts to detents (4 counts per detent in encoder mode 3)");
    encoder_t e = mk(12u, 6u);

    ck("1 count does not move a detent", !encoder_apply_delta(&e, 1));
    ck("2 counts still nothing",         !encoder_apply_delta(&e, 1));
    ck("3 counts still nothing",         !encoder_apply_delta(&e, 1));
    ck("4th count advances one detent",   encoder_apply_delta(&e, 1));
    ck_i("position after one detent up", e.position, 7);

    /* Symmetry: the same must hold turning down. A truncation mistake makes one
     * direction click a step early, which feels like backlash. */
    ck("down 1 count: nothing", !encoder_apply_delta(&e, -1));
    ck("down 2: nothing",       !encoder_apply_delta(&e, -1));
    ck("down 3: nothing",       !encoder_apply_delta(&e, -1));
    ck("down 4: one detent",     encoder_apply_delta(&e, -1));
    ck_i("back where we started", e.position, 6);
}

static void test_endstops(void)
{
    puts("\nend-stops: CLAMP, never wrap (these are trim controls)");
    encoder_t e = mk(12u, 0u);

    turn(&e, -5);
    ck_i("cannot go below zero", e.position, 0);

    e = mk(12u, 11u);
    turn(&e, 5);
    ck_i("cannot exceed detents-1", e.position, 11);

    /* The specific catastrophe: one extra click at the top must not become
     * position 0. On a fuel trim that is full-rich to full-lean in one click. */
    e = mk(12u, 11u);
    turn(&e, 1);
    ck("one click past maximum does NOT wrap to zero", e.position == 11u);

    e = mk(12u, 0u);
    turn(&e, -1);
    ck("one click below zero does NOT wrap to maximum", e.position == 0u);

    /* No wind-up: turning ten clicks past the stop then one back must move one
     * step, not sit dead for ten clicks while an internal counter unwinds. */
    e = mk(12u, 11u);
    turn(&e, 10);
    ck_i("still clamped after ten extra clicks", e.position, 11);
    turn(&e, -1);
    ck_i("one click back moves immediately (no wind-up)", e.position, 10);
}

static void test_full_travel(void)
{
    puts("\nfull travel and AVI mapping");
    encoder_t e = mk(ENC_DETENTS_THUMB, 0u);
    turn(&e, (int)ENC_DETENTS_THUMB - 1);
    ck_i("12-detent knob reaches position 11", e.position, ENC_DETENTS_THUMB - 1u);
    ck_i("which maps to full-scale AVI",
         io12_position_to_avi(e.position, ENC_DETENTS_THUMB), IO12_AVI_FULL_SCALE);

    encoder_t f = mk(ENC_DETENTS_FACEPLATE, 0u);
    turn(&f, (int)ENC_DETENTS_FACEPLATE - 1);
    ck_i("20-detent knob reaches position 19", f.position, ENC_DETENTS_FACEPLATE - 1u);
    ck_i("also full-scale AVI",
         io12_position_to_avi(f.position, ENC_DETENTS_FACEPLATE), IO12_AVI_FULL_SCALE);
}

static void test_change_flag(void)
{
    puts("\nchange flag");
    encoder_init();
    encoder_t *e = encoder_debug_state(0);
    ck("encoder 0 exists", e != NULL);
    if (!e) { return; }

    /* encoder_init() starts knobs mid-travel: a trim that boots hard against
     * zero is indistinguishable from one the driver wound fully down. */
    ck_i("boots mid-travel, not at an end-stop", e->position, ENC_DETENTS_THUMB / 2u);

    (void)encoder_take_change(0);                 /* clear the init flag */
    ck("no change reported when nothing moved", !encoder_take_change(0));

    (void)encoder_apply_delta(e, ENC_COUNTS_PER_DETENT);
    ck("change reported after a detent",  encoder_take_change(0));
    ck("flag cleared by reading it",     !encoder_take_change(0));

    /* Movement that does not complete a detent must not raise the flag --
     * otherwise every knob reports a change on contact chatter and the
     * firmware transmits AVI updates that contain no new information. */
    (void)encoder_apply_delta(e, 1);
    ck("sub-detent movement raises no change", !encoder_take_change(0));
}

static void test_bounds(void)
{
    puts("\nbounds and defensive cases");
    ck("NULL encoder is safe",        !encoder_apply_delta(NULL, 4));
    encoder_t z = mk(0u, 0u);
    ck("zero-detent encoder is safe", !encoder_apply_delta(&z, 4));

    encoder_init();
    ck_i("out-of-range index reads 0", encoder_position(ENC_COUNT + 5u), 0);
    ck("out-of-range change is false", !encoder_take_change(ENC_COUNT + 5u));
    encoder_set_position(ENC_COUNT + 5u, 3u);     /* must not corrupt memory */
    ck("out-of-range set is ignored", true);

    encoder_set_position(0, 200u);
    ck_i("set_position clamps to detents-1", encoder_position(0), ENC_DETENTS_THUMB - 1u);
}

int main(void)
{
    test_raw_delta_wrap();
    test_counts_per_detent();
    test_endstops();
    test_full_travel();
    test_change_flag();
    test_bounds();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
