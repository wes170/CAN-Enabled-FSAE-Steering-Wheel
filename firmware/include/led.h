/* led.h — WS2812B chain: 16 shift lights, 4 traction, 4 lockup.
 *
 * THE RULE FOR THIS MODULE: no code anywhere writes the strip directly.
 * Every pixel change goes through led_set() and every frame goes out through
 * led_show(), which applies the global current cap. This is a standing project
 * requirement (`engineering-rigor.md` §3), not a style preference — the cap is
 * the only thing standing between a pattern bug and a connector that carries
 * more current than the harness was sized for.
 */
#ifndef LED_H
#define LED_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

/* Wheel only. The dash's pin map reserves PA8/TIM1_CH1 for an off-board LED
 * alarm strip, but board_config.h does not yet define how many LEDs that strip
 * has -- and the current cap is meaningless without a count. Define
 * LED_TOTAL for the dash before extending this module to it, rather than
 * guessing a length here. */
#ifdef BOARD_WHEEL

/*  Per-channel current at full brightness.
 *
 *  ⚠ ASSUMPTION A3. The WS2812B-2020 datasheet is image-only and has never
 *  been text-extractable, so this is the accepted industry figure and NOT a
 *  datasheet read. It must be closed by measuring a real 24-LED strip at full
 *  white (staged bring-up step 7).
 *
 *  Note the cap does not depend on this being exactly right to be useful: if
 *  the true figure is higher, the cap still bounds the total, it just bounds
 *  it somewhere other than where we think. Getting A3 wrong makes the cap
 *  imprecise, not absent. */
#define LED_MA_PER_CHANNEL_FULL 12u     /* ~36 mA per LED at full white */

/* Quiescent draw of the controller in each LED, regardless of colour. Also
 * A3 — measured, not read. Included because 24 LEDs of quiescent current is
 * not negligible against a 450 mA cap. */
#define LED_MA_QUIESCENT_PER_LED 1u

#if defined(BOARD_SIM)
#  define LED_CURRENT_CAP_MA LED_CURRENT_CAP_MA_SIM
#else
#  define LED_CURRENT_CAP_MA LED_CURRENT_CAP_MA_CAR
#endif

typedef struct { uint8_t g, r, b; } led_rgb_t;   /* WS2812 wire order is GRB */

void led_init(void);

/* Set one pixel in the working buffer. Out-of-range indices are ignored. */
void led_set(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/* Set every pixel. */
void led_fill(uint8_t r, uint8_t g, uint8_t b);

/* Apply the current cap and start the DMA transfer. */
void led_show(void);

/* ---------------------------------------------------------------------------
 * Host-testable core
 * ------------------------------------------------------------------------ */

/*  Estimated current in mA for a frame, including quiescent draw. */
uint32_t led_estimate_ma(const led_rgb_t *px, uint8_t count);

/*  Scale a frame down, in place, until its estimate fits under `cap_ma`.
 *  Returns the scale factor applied in 1/256ths (256 = untouched).
 *
 *  Scaling is PROPORTIONAL across the whole frame rather than truncating the
 *  brightest pixels: the shift lights are read as a bar, and dimming only the
 *  bright end would change which segment looks "lit" — the driver would read
 *  a different rev point. Uniform dimming keeps the pattern's meaning and only
 *  costs brightness. */
uint16_t led_apply_cap(led_rgb_t *px, uint8_t count, uint32_t cap_ma);

/* Test-only access to the working buffer. */
led_rgb_t *led_debug_buffer(void);

#endif /* BOARD_WHEEL */

#endif /* LED_H */
