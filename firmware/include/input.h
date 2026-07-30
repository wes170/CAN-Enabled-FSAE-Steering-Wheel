/* input.h — debounced switches: encoder pushes and buttons.
 *
 * All inputs are ACTIVE LOW: external 10 kΩ pull-up to +3V3, switch shorts to
 * GND, with a 1 kΩ/100 nF RC at the pin. The pull-ups are external rather than
 * the MCU's internal ones on purpose — an external pull-up holds a defined
 * level during reset and while the pin is still an input-floating GPIO, so the
 * line never floats into an undefined state at power-on.
 *
 * The RC is asymmetric by construction: falling through 1 kΩ is ~100 µs,
 * rising back through 10 kΩ is ~1 ms. Debounce has to be comfortably longer
 * than both, and longer still than the mechanical chatter on top.
 */
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

/*  Debounce by INTEGRATION, not by N-consecutive-samples.
 *
 *  A consecutive-samples filter resets its whole count on a single stray
 *  sample, so under continuous vibration — which is the normal condition for a
 *  steering wheel on a race car, not an edge case — it can stall and never
 *  confirm a state at all. An integrator instead moves a counter up while the
 *  pin reads pressed and down while it reads released, and only changes its
 *  mind at the thresholds. Isolated noise samples nudge the count and lose.
 *
 *  Separate press and release thresholds give hysteresis: once a state is
 *  confirmed, it takes the full count in the other direction to undo it.
 */
#define DEBOUNCE_MAX_COUNT    20u   /* samples at 1 kHz, so 20 ms of travel */
#define DEBOUNCE_PRESS_COUNT  20u   /* integrator must reach max to confirm  */
#define DEBOUNCE_RELEASE_COUNT 0u   /* and fall to zero to release           */

typedef struct {
    uint8_t count;      /* integrator, 0 .. DEBOUNCE_MAX_COUNT */
    bool    state;      /* debounced: true = pressed           */
    bool    edge_press; /* set on release->press, consumed by reader */
} debounced_input_t;

/*  Feed one raw sample. `raw_pressed` is the ALREADY-INVERTED logical level
 *  (true = switch closed = pin low), so the active-low inversion happens once,
 *  at the pin read, and never again. Returns true if the debounced state
 *  changed on this sample. */
bool debounce_update(debounced_input_t *d, bool raw_pressed);

/* ---------------------------------------------------------------------------
 * Board-level API
 * ------------------------------------------------------------------------ */

#ifdef BOARD_WHEEL
#  define INPUT_ENC_SW_COUNT ENC_COUNT
#  define INPUT_BTN_COUNT    BTN_COUNT
#else
#  define INPUT_ENC_SW_COUNT 0u
#  define INPUT_BTN_COUNT    BTN_COUNT
#endif
#define INPUT_TOTAL (INPUT_ENC_SW_COUNT + INPUT_BTN_COUNT)

void input_init(void);
void input_task_1ms(void);

/* Debounced level. Index 0..INPUT_ENC_SW_COUNT-1 are encoder pushes (wheel
 * only), the rest are buttons. */
bool input_is_pressed(uint8_t index);

/* True once per press; reading it clears the edge. */
bool input_take_press(uint8_t index);

/*  Pack the debounced states into the 12-bit field the CANopen keypad frame
 *  wants: bit N = key N+1. Bounded to 12 keys because that is what the frame
 *  holds — a 13th input would silently vanish, so it is clamped explicitly. */
uint16_t input_keypad_bits(void);

/* Test-only. */
debounced_input_t *input_debug_state(uint8_t index);

#endif /* INPUT_H */
