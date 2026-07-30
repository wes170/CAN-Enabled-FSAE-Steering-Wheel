/* encoder.h — six detented rotary encoders, hardware quadrature decode.
 *
 * The position logic is separated from the timer registers so it can be tested
 * on a host. That separation is not ceremony: the counts-per-detent and
 * end-stop behaviour below are the parts that produce a *wrong trim value sent
 * to the ECU*, which is the failure mode with actual consequences.
 */
#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_WHEEL

/*  Counts per detent.
 *
 *  In the STM32's TI1+TI2 encoder mode the counter advances on BOTH edges of
 *  BOTH channels, so one mechanical detent of a standard quadrature encoder
 *  produces FOUR counts. Dividing by this is what turns raw counts into clicks.
 *
 *  ⚠ This is a property of the specific encoder, not a universal constant.
 *  Some detented encoders rest between quadrature states and give 2 counts per
 *  detent, or 1. Getting it wrong does not fail loudly -- the knob simply feels
 *  like it takes four clicks to move one step, or jumps four steps per click.
 *  CONFIRM ON THE BENCH at bring-up (staged bring-up step 6) and correct here. */
#define ENC_COUNTS_PER_DETENT 4

/*  End-stop behaviour: CLAMP, never wrap.
 *
 *  These knobs are trim controls -- the ECU reads their position as a fuel or
 *  boost adjustment. If position wrapped from maximum back to zero, one extra
 *  click at the end of travel would command a jump from one extreme of the trim
 *  range to the other. On a running engine that is a real event, not a UI
 *  annoyance. A detented knob has no physical stop to prevent the extra click,
 *  so the stop has to exist in software. */

typedef struct {
    int32_t  count;        /* accumulated quadrature counts, signed          */
    uint8_t  position;     /* detent index, 0 .. detents-1, always clamped   */
    uint8_t  detents;      /* travel range for this encoder                  */
    uint16_t last_raw;     /* previous timer CNT, for delta extraction       */
    bool     changed;      /* set on position change, cleared by the reader  */
} encoder_t;

/* Configure the six timers for quadrature mode and zero the state. */
void encoder_init(void);

/* Poll all six timers and update positions. Call at 1 kHz. */
void encoder_task_1ms(void);

/* Current detent index, 0 .. detents-1. */
uint8_t encoder_position(uint8_t index);

/* True once per position change; reading it clears the flag. */
bool encoder_take_change(uint8_t index);

/* Force a position, e.g. restoring a saved value at boot. Clamped. */
void encoder_set_position(uint8_t index, uint8_t position);

/* ---------------------------------------------------------------------------
 * Host-testable core. encoder_apply_delta() contains all the logic that can be
 * wrong; encoder_task_1ms() only supplies it with hardware counter deltas.
 * ------------------------------------------------------------------------ */

/*  Extract the signed movement between two readings of a 16-bit up/down
 *  counter. Must be correct across the counter's wrap in both directions:
 *  the counter is free-running, so a knob turned down through zero reads
 *  0x0002 -> 0xFFFE, which is -4, not +65532. */
int16_t encoder_raw_delta(uint16_t previous, uint16_t current);

/*  Apply a count delta to an encoder, converting counts to detents and
 *  clamping at both ends. Returns true if the detent position changed. */
bool encoder_apply_delta(encoder_t *e, int16_t delta_counts);

/* Test-only: direct access to internal state, so host tests exercise the same
 * array the firmware uses rather than a copy. */
encoder_t *encoder_debug_state(uint8_t index);

#endif /* BOARD_WHEEL */

#endif /* ENCODER_H */
