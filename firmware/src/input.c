/* input.c — debounced switch reading for both boards.
 *
 * The debounce logic is a pure function so it can be tested against synthetic
 * bounce and vibration patterns on a host. That is the whole reason it is
 * factored this way: switch bounce is easy to get subtly wrong and impossible
 * to observe reliably on a bench, because the failure is a missed or doubled
 * press once every few hundred operations.
 */

#include "input.h"
#include <string.h>

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

/* ---------------------------------------------------------------------------
 * Pure debounce
 * ------------------------------------------------------------------------ */

bool debounce_update(debounced_input_t *d, bool raw_pressed)
{
    if (!d) { return false; }

    if (raw_pressed) {
        if (d->count < DEBOUNCE_MAX_COUNT) { d->count++; }
    } else {
        if (d->count > 0u) { d->count--; }
    }

    const bool before = d->state;

    /* Hysteresis: confirm a press only at the top of the integrator and a
     * release only at the bottom. Between the two thresholds the state is
     * held, which is what makes continuous chatter around the midpoint
     * produce no output at all rather than a burst of transitions. */
    if (!d->state && d->count >= DEBOUNCE_PRESS_COUNT) {
        d->state = true;
    } else if (d->state && d->count <= DEBOUNCE_RELEASE_COUNT) {
        d->state = false;
    }

    if (d->state && !before) {
        d->edge_press = true;       /* consumed by input_take_press() */
    }
    return d->state != before;
}

/* ---------------------------------------------------------------------------
 * Board wiring
 * ------------------------------------------------------------------------ */

typedef struct { void *port; uint8_t pin; } pin_t;

static debounced_input_t s_in[INPUT_TOTAL];

#ifndef FIRMWARE_HOST_BUILD
static const pin_t k_pins[INPUT_TOTAL] = {
#  ifdef BOARD_WHEEL
    /* Encoder push switches first, then buttons — the order the keypad frame
     * expects, and the order input_is_pressed() indexes. */
    {GPIOC, 4}, {GPIOC, 5}, {GPIOC, 8}, {GPIOC, 9}, {GPIOC, 10}, {GPIOC, 11},
    {GPIOC, 13}, {GPIOD, 2}, {GPIOA, 3}, {GPIOB, 10}, {GPIOB, 11}, {GPIOB, 12},
#  else
    {GPIOC, 8}, {GPIOC, 9}, {GPIOC, 10},
#  endif
};

static bool pin_is_pressed(const pin_t *p)
{
    /* ACTIVE LOW — invert exactly here, once. Every layer above this one deals
     * in logical "pressed", never in pin levels. */
    GPIO_TypeDef *g = (GPIO_TypeDef *)p->port;
    return (g->IDR & (1u << p->pin)) == 0u;
}
#endif

void input_init(void)
{
    memset(s_in, 0, sizeof s_in);

    /* Deliberately NOT seeded from the current pin state. Sampling once at
     * boot and trusting it would latch a spurious press if a line happens to
     * be low during power-up — and on the wheel a held button at boot is
     * exactly what a sticky switch or a wiring fault looks like. Starting at
     * zero means a genuinely held button takes 20 ms to register, which is
     * imperceptible, and a fault registers as nothing rather than as input. */

#ifndef FIRMWARE_HOST_BUILD
    /* Pins are plain inputs. No internal pull-up is enabled: the 10 kΩ
     * external pull-up is the defined-state mechanism (it works during reset,
     * when the internal one does not), and enabling both would fight the RC
     * time constant the debounce timing above is derived from. */
#endif
}

void input_task_1ms(void)
{
#ifndef FIRMWARE_HOST_BUILD
    for (unsigned i = 0; i < INPUT_TOTAL; ++i) {
        (void)debounce_update(&s_in[i], pin_is_pressed(&k_pins[i]));
    }
#endif
}

bool input_is_pressed(uint8_t index)
{
    return (index < INPUT_TOTAL) ? s_in[index].state : false;
}

bool input_take_press(uint8_t index)
{
    if (index >= INPUT_TOTAL) { return false; }
    const bool e = s_in[index].edge_press;
    s_in[index].edge_press = false;
    return e;
}

uint16_t input_keypad_bits(void)
{
    uint16_t bits = 0u;
    /* The CANopen keypad frame carries 12 keys. Clamp rather than let a 13th
     * input silently disappear into a shift that falls off the end. */
    const unsigned n = (INPUT_TOTAL < 12u) ? INPUT_TOTAL : 12u;
    for (unsigned i = 0; i < n; ++i) {
        if (s_in[i].state) { bits |= (uint16_t)(1u << i); }
    }
    return bits;
}

debounced_input_t *input_debug_state(uint8_t index)
{
    return (index < INPUT_TOTAL) ? &s_in[index] : (debounced_input_t *)0;
}
