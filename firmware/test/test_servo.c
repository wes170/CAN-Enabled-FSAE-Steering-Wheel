/* test_servo.c — host tests for the four ARB safety rules.
 *
 * Every test here corresponds to a rule in engineering-rigor.md §3, and each
 * is written so that DELETING the rule from servo.c makes it fail. That is the
 * point: these rules protect against a vehicle-dynamics event, and the most
 * likely way to lose one is a well-meaning refactor, not a typo.
 */
#include <stdio.h>
#include <string.h>
#include "servo.h"

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

/* Advance the task by n milliseconds from a base time, with NO commands --
 * i.e. deliberately letting the CAN-loss timer run. */
static uint32_t run_ms(uint32_t t0, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) { servo_task_1ms(t0 + i); }
    return t0 + n;
}

/* Advance while keeping the command stream alive, which is what normal
 * operation looks like. Without this, rule 1 fires 500 ms in and every other
 * assertion in the test is really testing the CAN-loss path by accident. */
static uint32_t run_ms_commanded(uint32_t t0, unsigned n, uint16_t pos)
{
    for (unsigned i = 0; i < n; ++i) {
        servo_command(0, pos, t0 + i);
        /* Feedback must be supplied too. Without it, measured_us sits at its
         * initial value while output_us slews away, and rule 4 correctly
         * raises a divergence alarm -- which is right, and would silently make
         * every other assertion in the test read the wrong status. */
        servo_feedback(0, pos, t0 + i);
        servo_task_1ms(t0 + i);
    }
    return t0 + n;
}

/* ======================================================================== */

static void test_rule2_endstops(void)
{
    puts("\nRULE 2 — end-stops (a servo on a hard stop burns)");
    ck_u("position 0 -> minimum pulse",
         servo_position_to_us(0u), SERVO_PULSE_MIN_US);
    ck_u("position 4095 -> maximum pulse",
         servo_position_to_us(4095u), SERVO_PULSE_MAX_US);
    ck_u("midpoint",
         servo_position_to_us(2047u),
         SERVO_PULSE_MIN_US + (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) * 2047u / 4095u);

    /* A corrupt or out-of-range CAN value must land INSIDE the travel. */
    ck_u("out-of-range 9999 clamps to maximum",
         servo_position_to_us(9999u), SERVO_PULSE_MAX_US);
    ck_u("0xFFFF clamps to maximum",
         servo_position_to_us(0xFFFFu), SERVO_PULSE_MAX_US);

    /* Sweep the whole input range: nothing may ever escape the end-stops. */
    bool all_inside = true;
    for (uint32_t p = 0; p <= 0xFFFFu; p += 7u) {
        const uint16_t us = servo_position_to_us((uint16_t)p);
        if (us < SERVO_PULSE_MIN_US || us > SERVO_PULSE_MAX_US) { all_inside = false; break; }
    }
    ck("no input anywhere in 0..65535 escapes the end-stops", all_inside);
}

static void test_rule3_slew(void)
{
    puts("\nRULE 3 — slew limiting (a knob spin must not slam a linkage)");

    /* THE TRAP this accumulator exists for: 500 us/s at a 1 ms tick is 0.5 us,
     * which truncates to zero in integer arithmetic. Without the accumulator
     * the servo never moves at all, and nothing reports an error. This was a
     * real bug in the first version of servo.c, caught here. */
    uint32_t acc = 0u;
    uint16_t v = 1000u;
    for (unsigned i = 0; i < 2u; ++i) { v = servo_slew(v, 2000u, 1u, &acc); }
    ck("two 1 ms ticks move exactly 1 us (fractions accumulate)", v == 1001u);
    printf("        after 2 x 1 ms ticks: 1000 -> %u us\n", v);

    acc = 0u;
    v = 1000u;
    for (unsigned i = 0; i < 2000u; ++i) { v = servo_slew(v, 2000u, 1u, &acc); }
    ck_u("2000 x 1 ms ticks cover the full 1000 us travel", v, 2000u);

    acc = 0u;
    ck_u("10 ms of slew moves 5 us", servo_slew(1000u, 2000u, 10u, &acc), 1005u);
    acc = 0u;
    ck_u("100 ms of slew moves 50 us", servo_slew(1000u, 2000u, 100u, &acc), 1050u);
    acc = 0u;
    ck_u("slews downward too", servo_slew(2000u, 1000u, 100u, &acc), 1950u);

    /* Never overshoot the target. */
    acc = 0u;
    ck_u("small gap upward is not overshot", servo_slew(1000u, 1002u, 1000u, &acc), 1002u);
    acc = 0u;
    ck_u("small gap downward is not overshot", servo_slew(1002u, 1000u, 1000u, &acc), 1000u);
    acc = 0u;
    ck_u("already at target", servo_slew(1500u, 1500u, 100u, &acc), 1500u);

    /* Sitting at the target must not bank credit for a free jump later. */
    acc = 0u;
    for (unsigned i = 0; i < 5000u; ++i) { (void)servo_slew(1500u, 1500u, 1u, &acc); }
    ck_u("no credit banked while parked at target", acc, 0u);
    ck_u("so the next move is still rate-limited",
         servo_slew(1500u, 2000u, 1u, &acc), 1500u);

    /* Full travel must take about the documented ~2 s. */
    const uint32_t span = SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US;
    const uint32_t secs = span / SERVO_SLEW_US_PER_S;
    printf("        full travel %u us at %u us/s = %u s\n",
           span, SERVO_SLEW_US_PER_S, secs);
    ck("full travel takes at least 1 second", secs >= 1u);
}

static void test_rule1_can_loss_holds(void)
{
    puts("\nRULE 1 — CAN loss HOLDS position (never springs to centre)");
    servo_init();

    /* Command hard over to one end and let it get there. */
    uint32_t t = run_ms_commanded(0u, 4000u, 4095u);
    const uint16_t settled = servo_output_us(0);
    ck_u("servo reached the commanded end", settled, SERVO_PULSE_MAX_US);
    ck("status OK while commands flow", servo_status(0) == SERVO_OK);

    /* Now stop commanding. Feed the feedback so divergence does not confuse
     * the picture — this test is about rule 1 alone. */
    servo_feedback(0, 4095u, t);
    t = run_ms(t, SERVO_CAN_TIMEOUT_MS + 100u);

    ck("status reports the CAN loss", servo_status(0) == SERVO_HOLDING_CAN_LOSS);
    ck_u("OUTPUT IS UNCHANGED — held, not centred", servo_output_us(0), settled);

    const uint16_t centre = (uint16_t)((SERVO_PULSE_MIN_US + SERVO_PULSE_MAX_US) / 2u);
    ck("and specifically did NOT go to centre", servo_output_us(0) != centre);

    /* Keep not commanding for a long time. It must still hold. */
    t = run_ms(t, 10000u);
    ck_u("still held after 10 more seconds", servo_output_us(0), settled);

    /* Recovery. */
    servo_command(0, 4095u, t);
    servo_task_1ms(t);
    ck("status recovers when commands resume", servo_status(0) == SERVO_OK);
}

static void test_rule4_divergence(void)
{
    puts("\nRULE 4 — divergence alarm (seized linkage / stripped spline)");
    servo_init();

    /* Command a move and have the feedback follow it correctly. */
    uint32_t t = 0u;
    for (unsigned i = 0; i < 3000u; ++i) {
        servo_command(0, 2047u, t);        /* keep CAN alive */
        servo_feedback(0, 2047u, t);
        servo_task_1ms(t);
        ++t;
    }
    ck("no alarm when the mechanism follows", servo_status(0) == SERVO_OK);

    /* Now the mechanism seizes: feedback stops changing while the command
     * moves away from it. */
    servo_init();
    t = 0u;
    servo_command(0, 4095u, t);
    for (unsigned i = 0; i < 200u; ++i) {
        servo_feedback(0, 2047u, t);       /* stuck at midpoint */
        servo_command(0, 4095u, t);        /* keep CAN alive so rule 1 stays out */
        servo_task_1ms(t);
        ++t;
    }
    /* Not yet — divergence must persist past the timeout before it alarms, so
     * a brief lag during a fast move does not cry wolf. */
    ck("brief mismatch does not alarm immediately", servo_status(0) != SERVO_DIVERGED);

    for (unsigned i = 0; i < SERVO_DIVERGENCE_TIMEOUT_MS + 2000u; ++i) {
        servo_feedback(0, 2047u, t);
        servo_command(0, 4095u, t);
        servo_task_1ms(t);
        ++t;
    }
    ck("sustained mismatch raises the alarm", servo_status(0) == SERVO_DIVERGED);

    /* The output must NOT be cut. A slow servo beats an ARB that goes limp. */
    ck("output still driven while diverged", servo_output_us(0) >= SERVO_PULSE_MIN_US);

    /* Recovery when the mechanism catches up. */
    for (unsigned i = 0; i < 100u; ++i) {
        servo_feedback(0, 4095u, t);
        servo_command(0, 4095u, t);
        servo_task_1ms(t);
        ++t;
    }
    ck("alarm clears when it catches up", servo_status(0) == SERVO_OK);
}

static void test_no_output_before_command(void)
{
    puts("\nsafety: nothing is driven before the first real command");
    servo_init();
    const uint16_t centre = (uint16_t)((SERVO_PULSE_MIN_US + SERVO_PULSE_MAX_US) / 2u);
    ck_u("initial output is mid-travel", servo_output_us(0), centre);

    /* No command yet. Even after the CAN timeout elapses, the channel must not
     * flip to a loss state — there was never a command to lose. */
    run_ms(0u, SERVO_CAN_TIMEOUT_MS + 500u);
    ck("no CAN-loss alarm before any command was received",
       servo_status(0) == SERVO_OK);
    ck_u("and the output has not moved", servo_output_us(0), centre);
}

static void test_bounds(void)
{
    puts("\nbounds");
    servo_init();
    servo_command(SERVO_COUNT + 2u, 4095u, 0u);      /* must not corrupt memory */
    servo_feedback(SERVO_COUNT + 2u, 4095u, 0u);
    ck("out-of-range channel ignored", servo_debug_state(SERVO_COUNT + 2u) == NULL);
    ck_u("out-of-range output reads 0", servo_output_us(SERVO_COUNT + 2u), 0u);

    /* Both channels are independent. */
    servo_init();
    servo_command(0, 4095u, 0u);
    uint32_t t = run_ms(0u, 3000u);
    ck("channel 0 moved", servo_output_us(0) == SERVO_PULSE_MAX_US);
    ck_u("channel 1 did not", servo_output_us(1),
         (uint16_t)((SERVO_PULSE_MIN_US + SERVO_PULSE_MAX_US) / 2u));
    (void)t;
}

static void test_rollover(void)
{
    puts("\nmillisecond rollover (49.7 days)");
    servo_init();
    const uint32_t near_wrap = 0xFFFFFF00u;
    servo_command(0, 4095u, near_wrap);
    /* Step across the wrap. Rule 1 must not fire spuriously. */
    for (unsigned i = 0; i < 200u; ++i) {
        servo_command(0, 4095u, (uint32_t)(near_wrap + i));
        servo_task_1ms((uint32_t)(near_wrap + i));
    }
    ck("no phantom CAN-loss across the uint32 wrap",
       servo_status(0) == SERVO_OK);
}

int main(void)
{
    test_rule2_endstops();
    test_rule3_slew();
    test_rule1_can_loss_holds();
    test_rule4_divergence();
    test_no_output_before_command();
    test_bounds();
    test_rollover();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
