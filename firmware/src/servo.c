/* servo.c — ARB servo outputs with the four safety rules.
 *
 * Each rule is a separate stage with its own test. That structure is
 * deliberate: a safety behaviour buried inside a larger function is a safety
 * behaviour that can be removed by someone refactoring for readability without
 * realising what they deleted.
 *
 * Read the reason before changing any constant in here (README rule 3).
 */

#include "servo.h"
#include <string.h>

#ifdef BOARD_DASH

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

static servo_channel_t s_ch[SERVO_COUNT];

/* ---------------------------------------------------------------------------
 * Rule 2 — end-stops
 * ------------------------------------------------------------------------ */

uint16_t servo_position_to_us(uint16_t position_0_4095)
{
    if (position_0_4095 > 4095u) { position_0_4095 = 4095u; }

    const uint32_t span = SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US;
    uint32_t us = SERVO_PULSE_MIN_US + ((uint32_t)position_0_4095 * span) / 4095u;

    /* Clamp again after the arithmetic. This is not redundant: it is the line
     * that holds if SERVO_PULSE_MIN/MAX are ever narrowed to the real
     * mechanical travel (which rule 2 requires before a servo touches a
     * linkage) and some other path computes a microsecond value directly. A
     * servo driven past its mechanical stop draws stall current until it
     * burns, so this clamp is the difference between a limit and a suggestion. */
    if (us < SERVO_PULSE_MIN_US) { us = SERVO_PULSE_MIN_US; }
    if (us > SERVO_PULSE_MAX_US) { us = SERVO_PULSE_MAX_US; }
    return (uint16_t)us;
}

/* ---------------------------------------------------------------------------
 * Rule 3 — slew limiting
 * ------------------------------------------------------------------------ */

uint16_t servo_slew(uint16_t current, uint16_t target, uint32_t dt_ms,
                    uint32_t *accum_milli)
{
    if (current == target) {
        /* At the target, drop any fractional credit. Letting it accumulate
         * would mean the next command starts with a free jump, which defeats
         * the point of a slew limit at exactly the moment it matters. */
        if (accum_milli) { *accum_milli = 0u; }
        return current;
    }

    /* Allowance in THOUSANDTHS of a microsecond, so that a 1 ms tick at
     * 500 us/s contributes 500 rather than truncating to zero. */
    uint32_t local = 0u;
    uint32_t *acc = accum_milli ? accum_milli : &local;

    *acc += (uint32_t)SERVO_SLEW_US_PER_S * dt_ms;

    const uint32_t whole_us = *acc / 1000u;
    if (whole_us == 0u) { return current; }
    *acc -= whole_us * 1000u;

    if (target > current) {
        const uint32_t gap = (uint32_t)target - current;
        return (uint16_t)(current + (gap < whole_us ? gap : whole_us));
    }
    const uint32_t gap = (uint32_t)current - target;
    return (uint16_t)(current - (gap < whole_us ? gap : whole_us));
}

/* ---------------------------------------------------------------------------
 * Command and feedback
 * ------------------------------------------------------------------------ */

void servo_init(void)
{
    memset(s_ch, 0, sizeof s_ch);
    for (unsigned i = 0; i < SERVO_COUNT; ++i) {
        /* Start at the centre of travel and, critically, with have_command
         * false — so nothing is driven until a real command arrives. The
         * centre is the safe *initial* position; it is never a position we
         * RETURN to (that is rule 1). */
        const uint16_t centre = (uint16_t)((SERVO_PULSE_MIN_US + SERVO_PULSE_MAX_US) / 2u);
        s_ch[i].commanded_us = centre;
        s_ch[i].output_us    = centre;
        s_ch[i].measured_us  = centre;
        s_ch[i].status       = SERVO_OK;
    }
}

void servo_command(uint8_t ch, uint16_t position_0_4095, uint32_t now_ms)
{
    if (ch >= SERVO_COUNT) { return; }
    s_ch[ch].commanded_us = servo_position_to_us(position_0_4095);
    s_ch[ch].last_cmd_ms  = now_ms;
    s_ch[ch].have_command = true;
    if (s_ch[ch].status == SERVO_HOLDING_CAN_LOSS) {
        s_ch[ch].status = SERVO_OK;      /* command stream recovered */
    }
}

void servo_feedback(uint8_t ch, uint16_t position_0_4095, uint32_t now_ms)
{
    (void)now_ms;
    if (ch >= SERVO_COUNT) { return; }
    s_ch[ch].measured_us = servo_position_to_us(position_0_4095);
}

/* ---------------------------------------------------------------------------
 * The 1 kHz task — rules 1, 3 and 4
 * ------------------------------------------------------------------------ */

void servo_task_1ms(uint32_t now_ms)
{
    for (unsigned i = 0; i < SERVO_COUNT; ++i) {
        servo_channel_t *c = &s_ch[i];

        if (!c->have_command) { continue; }   /* nothing commanded yet */

        /* --- Rule 1: CAN loss. HOLD, do not centre. ---------------------
         * Unsigned subtraction so this stays correct across the uint32
         * millisecond rollover. Note what this branch does NOT do: it does not
         * change output_us. Holding is the absence of an action, and it is
         * written explicitly so nobody later "fixes" the apparently missing
         * failsafe by adding a return-to-centre. An ARB stiffness step change
         * mid-corner is exactly the event this prevents. */
        if ((uint32_t)(now_ms - c->last_cmd_ms) >= SERVO_CAN_TIMEOUT_MS) {
            c->status = SERVO_HOLDING_CAN_LOSS;
            /* output_us deliberately untouched — hold the last position. */
        }

        /* --- Rule 3: slew toward the commanded position ------------------
         * Still applied while holding: if the command stream drops mid-slew,
         * the output continues gently to the last commanded value rather than
         * stopping part-way, which would leave the two bars mismatched. */
        c->output_us = servo_slew(c->output_us, c->commanded_us, 1u,
                                  &c->slew_accum_milli);

        /* --- Rule 4: divergence alarm ------------------------------------
         * Compared against output_us, NOT commanded_us. The servo is expected
         * to lag a slewing command — comparing to the command would raise an
         * alarm on every normal movement, and an alarm that cries wolf is an
         * alarm that gets ignored. output_us is what we are actually asking
         * the servo to do right now. */
        const uint16_t a = c->output_us, b = c->measured_us;
        const uint16_t err = (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);

        if (err > SERVO_DIVERGENCE_US) {
            if (!c->diverged_timing) {
                c->diverged_timing   = true;
                c->diverged_since_ms = now_ms;
            } else if ((uint32_t)(now_ms - c->diverged_since_ms)
                       >= SERVO_DIVERGENCE_TIMEOUT_MS) {
                /* Sustained mismatch: a seized linkage, a stripped spline, or
                 * a dead servo. Flag it; the dash shows it and the log records
                 * it. We do NOT cut the output — a servo that is merely slow
                 * is better than an ARB that goes limp mid-corner. */
                c->status = SERVO_DIVERGED;
            }
        } else {
            c->diverged_timing = false;
            if (c->status == SERVO_DIVERGED) { c->status = SERVO_OK; }
        }

#ifndef FIRMWARE_HOST_BUILD
        /* Timer is configured for 1 µs ticks, so the compare value IS the
         * pulse width in microseconds. */
        if (i == 0u) { SERVO_TIM->CCR1 = c->output_us; }
        else         { SERVO_TIM->CCR2 = c->output_us; }
#endif
    }
}

servo_status_t servo_status(uint8_t ch)
{
    return (ch < SERVO_COUNT) ? s_ch[ch].status : SERVO_OK;
}

uint16_t servo_output_us(uint8_t ch)
{
    return (ch < SERVO_COUNT) ? s_ch[ch].output_us : 0u;
}

servo_channel_t *servo_debug_state(uint8_t ch)
{
    return (ch < SERVO_COUNT) ? &s_ch[ch] : (servo_channel_t *)0;
}

#endif /* BOARD_DASH */
