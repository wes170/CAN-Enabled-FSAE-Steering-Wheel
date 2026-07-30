/* servo.h — anti-roll-bar servo outputs, two channels.
 *
 * This is the only module on either board whose failure modes are a VEHICLE
 * DYNAMICS event rather than a wrong number on a screen. An anti-roll bar
 * changes how the car takes a corner. A step change in ARB stiffness while the
 * car is loaded up mid-corner is a handling event the driver did not ask for.
 *
 * The four rules below are requirements from `engineering-rigor.md` §3 and
 * `board_config.h`. They are not tuning parameters. Each one is implemented as
 * a separate, individually testable stage, and each has a test that fails if
 * the stage is removed:
 *
 *   1. CAN loss  -> HOLD the last commanded position. Never centre.
 *   2. End-stops -> pulse width clamped to proven mechanical travel.
 *   3. Slew      -> limited rate of change, so a knob spin cannot slam a linkage.
 *   4. Divergence-> commanded vs measured mismatch raises an alarm.
 */
#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_DASH

#define SERVO_COUNT 2u

typedef enum {
    SERVO_OK = 0,
    SERVO_HOLDING_CAN_LOSS,   /* rule 1 active: command stream went away   */
    SERVO_DIVERGED            /* rule 4 tripped: mechanism is not following */
} servo_status_t;

typedef struct {
    uint16_t commanded_us;    /* what the CAN command asked for, clamped   */
    uint16_t output_us;       /* what we are actually driving, slew-limited */
    uint16_t measured_us;     /* feedback, converted to equivalent us       */
    uint32_t last_cmd_ms;     /* for rule 1                                 */
    uint32_t diverged_since_ms;
    uint32_t slew_accum_milli;/* thousandths of a us, carried between ticks */
    bool     diverged_timing; /* divergence currently being timed           */
    bool     have_command;    /* false until the first valid command        */
    servo_status_t status;
} servo_channel_t;

void servo_init(void);

/*  A new position command, 0..4095 as it arrives over CAN (the same scale the
 *  IO12 AVI channels use, so the wheel's knob position maps straight through
 *  with no second encoding to get wrong). Clamped to the end-stops here. */
void servo_command(uint8_t ch, uint16_t position_0_4095, uint32_t now_ms);

/*  Feedback from the ARB position sensors on AIN7/AIN8, 0..4095. */
void servo_feedback(uint8_t ch, uint16_t position_0_4095, uint32_t now_ms);

/* Run the slew limiter, the CAN-loss check and the divergence check, then
 * update the timer compare registers. Call at 1 kHz. */
void servo_task_1ms(uint32_t now_ms);

servo_status_t servo_status(uint8_t ch);
uint16_t servo_output_us(uint8_t ch);

/* ---------------------------------------------------------------------------
 * Host-testable stages
 * ------------------------------------------------------------------------ */

/*  Rule 2. Map 0..4095 onto the end-stops and clamp. The clamp is not
 *  redundant with the mapping: a corrupt or out-of-range CAN value must land
 *  inside the travel, not outside it. */
uint16_t servo_position_to_us(uint16_t position_0_4095);

/*  Rule 3. Move `current` toward `target` by at most the slew allowance for
 *  `dt_ms`, and return the new value.
 *
 *  `accum_milli` carries fractional movement between calls, in THOUSANDTHS of
 *  a microsecond. It is a parameter rather than a hidden static because of the
 *  bug it exists to prevent: at the 1 ms task rate the allowance is
 *  500 us/s x 1 ms = 0.5 us, which truncates to ZERO in integer arithmetic. A
 *  version without this accumulator compiles, reads correctly, passes a casual
 *  review — and never moves the servo at all. Found by the host tests, which
 *  is the entire argument for having them.
 *
 *  Pass a pointer to a zero-initialised uint32_t; the function maintains it. */
uint16_t servo_slew(uint16_t current, uint16_t target, uint32_t dt_ms,
                    uint32_t *accum_milli);

/*  Divergence threshold in microseconds. Wide enough not to trip on normal
 *  lag while the servo catches up to a slewing command, narrow enough to
 *  catch a mechanism that has stopped moving. */
#define SERVO_DIVERGENCE_US 250u

servo_channel_t *servo_debug_state(uint8_t ch);

#endif /* BOARD_DASH */
#endif /* SERVO_H */
