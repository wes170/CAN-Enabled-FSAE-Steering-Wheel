/* encoder.c — six detented encoders on six hardware quadrature decoders.
 *
 * The MCU's timers do the decoding, which matters for more than CPU load: a
 * hardware decoder cannot miss an edge or mis-order two edges that arrive
 * microseconds apart. A software-polled decoder on six channels would lose
 * counts during a fast flick of a thumb wheel, and lost counts on a trim
 * control mean the ECU's idea of the trim silently drifts away from the
 * driver's.
 *
 * ⚠ CORRECTED: an earlier version of this comment said the timer "filters the
 * illegal state transitions that contact bounce produces". That names the
 * wrong mechanism. The Bourns datasheets specify 5.0 ms of contact bounce
 * (PEC09) and 3.0 ms (PEC11H) -- far longer than either the conditioning RC
 * (1 ms rising) or the timer's input filter (IC1F = 0b1111, about 1.5 us at
 * 170 MHz). Neither of those is what defeats mechanical bounce.
 *
 * What defeats it is QUADRATURE ITSELF. The count direction depends on the
 * RELATIVE state of A and B, so while one contact is chattering the other is
 * stable: the counter steps up and down alternately and lands back exactly
 * where it started. Bounce cancels by construction. That self-cancellation is
 * the real reason to decode in hardware, and the RC and the input filter are
 * there for electrical noise.
 *
 * Everything here that can be *wrong* -- counts-to-detents, wrap handling,
 * end-stops -- is in pure functions with no register access, so it is tested
 * on a host. See firmware/test/test_encoder.c.
 */

#include "encoder.h"
#include <string.h>

/* Encoders exist only on the wheel. The dash has three buttons and no rotary
 * controls, so this whole translation unit compiles away there -- and the
 * both-boards compile check in run-tests.sh is what proved that was needed. */
#ifdef BOARD_WHEEL

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

static encoder_t s_enc[ENC_COUNT];

/* Which encoders are thumb wheels and which are faceplate knobs. They have
 * different detent counts, so they map to different AVI scalings. */
static const uint8_t k_detents[ENC_COUNT] = {
    ENC_DETENTS_THUMB, ENC_DETENTS_THUMB, ENC_DETENTS_THUMB, ENC_DETENTS_THUMB,
    ENC_DETENTS_FACEPLATE, ENC_DETENTS_FACEPLATE
};

/* ---------------------------------------------------------------------------
 * Pure logic
 * ------------------------------------------------------------------------ */

int16_t encoder_raw_delta(uint16_t previous, uint16_t current)
{
    /* The subtraction is done in 16-bit unsigned, then reinterpreted as
     * signed. That is what makes the wrap work: 0x0002 - 0xFFFE = 0x0004 going
     * up, and 0xFFFE - 0x0002 = 0xFFFC = -4 going down. Widening to int32
     * first would produce +65532 and a knob that leaps a full turn when it
     * crosses zero. */
    return (int16_t)(uint16_t)(current - previous);
}

bool encoder_apply_delta(encoder_t *e, int16_t delta_counts)
{
    if (!e || e->detents == 0u) { return false; }

    const uint8_t before = e->position;
    e->count += delta_counts;

    /* Integer division truncates toward zero in C, which is what we want here:
     * partial movement within a detent must not register until the detent is
     * actually reached, in EITHER direction. Truncation toward zero gives that
     * symmetry; floor division would make one direction click a step early. */
    int32_t detent_delta = e->count / ENC_COUNTS_PER_DETENT;

    if (detent_delta != 0) {
        /* Keep only the remainder, so fractional movement is never lost. */
        e->count -= detent_delta * ENC_COUNTS_PER_DETENT;

        int32_t p = (int32_t)e->position + detent_delta;

        /* CLAMP, never wrap -- see the note in encoder.h. One extra click at
         * the end of travel must do nothing, not jump the trim to the opposite
         * extreme. */
        if (p < 0) { p = 0; }
        if (p > (int32_t)(e->detents - 1u)) { p = (int32_t)(e->detents - 1u); }

        e->position = (uint8_t)p;
    }

    if (e->position != before) {
        e->changed = true;
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Accessors
 * ------------------------------------------------------------------------ */

uint8_t encoder_position(uint8_t index)
{
    return (index < ENC_COUNT) ? s_enc[index].position : 0u;
}

bool encoder_take_change(uint8_t index)
{
    if (index >= ENC_COUNT) { return false; }
    const bool c = s_enc[index].changed;
    s_enc[index].changed = false;
    return c;
}

void encoder_set_position(uint8_t index, uint8_t position)
{
    if (index >= ENC_COUNT) { return; }
    encoder_t *e = &s_enc[index];
    if (position > e->detents - 1u) { position = (uint8_t)(e->detents - 1u); }
    e->position = position;
    e->count    = 0;
    e->changed  = true;
}

/* ---------------------------------------------------------------------------
 * Hardware
 * ------------------------------------------------------------------------ */

#ifndef FIRMWARE_HOST_BUILD
static TIM_TypeDef *const k_tim[ENC_COUNT] = {
    ENC1_TIM, ENC2_TIM, ENC3_TIM, ENC4_TIM, ENC5_TIM, ENC6_TIM
};

static void encoder_timer_init(TIM_TypeDef *t)
{
    t->CR1  = 0u;                       /* stop while reconfiguring          */

    /* Both channels as inputs mapped to their own inputs. */
    t->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0
             /* Input filter at maximum: fSAMPLING = fDTS/32, N = 8, so it
              * rejects pulses shorter than about 1.5 us at 170 MHz. That is
              * for ELECTRICAL noise -- it is three orders of magnitude short
              * of the datasheets' 3-5 ms of mechanical bounce, which
              * quadrature cancels on its own (see the file header). It costs
              * nothing, so it is set to maximum. */
             | (0x0Fu << TIM_CCMR1_IC1F_Pos)
             | (0x0Fu << TIM_CCMR1_IC2F_Pos);

    t->CCER = 0u;                       /* both edges, non-inverted          */

    /* SMS = 011: encoder mode 3 -- count on both edges of both channels.
     * This is the mode that gives four counts per detent, which is what
     * ENC_COUNTS_PER_DETENT is compensating for. */
    t->SMCR = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;

    t->ARR = 0xFFFFu;                   /* free-running full 16-bit range    */
    t->CNT = 0u;
    t->CR1 = TIM_CR1_CEN;
}
#endif

void encoder_init(void)
{
    memset(s_enc, 0, sizeof s_enc);
    for (unsigned i = 0; i < ENC_COUNT; ++i) {
        s_enc[i].detents = k_detents[i];
        /* Start mid-travel rather than at an end-stop. A trim knob that boots
         * hard against zero is indistinguishable from one the driver wound
         * fully down, and the ECU cannot tell the difference either. */
        s_enc[i].position = (uint8_t)(k_detents[i] / 2u);
    }
#ifndef FIRMWARE_HOST_BUILD
    for (unsigned i = 0; i < ENC_COUNT; ++i) {
        encoder_timer_init(k_tim[i]);
        s_enc[i].last_raw = (uint16_t)k_tim[i]->CNT;
    }
#endif
}

void encoder_task_1ms(void)
{
#ifndef FIRMWARE_HOST_BUILD
    for (unsigned i = 0; i < ENC_COUNT; ++i) {
        const uint16_t raw = (uint16_t)k_tim[i]->CNT;
        const int16_t  d   = encoder_raw_delta(s_enc[i].last_raw, raw);
        s_enc[i].last_raw  = raw;
        if (d != 0) { (void)encoder_apply_delta(&s_enc[i], d); }
    }
#endif
}

/* Test-only access to the internal array, so the host tests exercise the same
 * state the firmware uses rather than a copy. */
encoder_t *encoder_debug_state(uint8_t index)
{
    return (index < ENC_COUNT) ? &s_enc[index] : (encoder_t *)0;
}

#endif /* BOARD_WHEEL */
