/* led.c — WS2812B chain via TIM16 PWM + DMA, behind a global current cap.
 *
 * Why PWM + DMA rather than bit-banging: the WS2812 encodes each bit as a pulse
 * width, with roughly 1.25 µs per bit and tolerances of a few hundred
 * nanoseconds. Bit-banging 24 LEDs means 576 bits of cycle-counted output with
 * interrupts disabled for ~720 µs. On a board that must also service a 1 Mbit
 * CAN bus, that is 720 µs of latency added to every frame at whatever rate the
 * LEDs refresh. DMA into the timer's compare register costs no CPU time at all
 * and cannot be perturbed by an interrupt arriving mid-frame.
 *
 * The current cap is the part of this file that matters most. See led.h.
 */

#include "led.h"
#include "power.h"
#include <string.h>

#ifdef BOARD_WHEEL

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

static led_rgb_t s_px[LED_TOTAL];

/* ---------------------------------------------------------------------------
 * Current estimation and capping — pure, host-tested
 * ------------------------------------------------------------------------ */

uint32_t led_estimate_ma(const led_rgb_t *px, uint8_t count)
{
    if (!px) { return 0u; }

    /* Sum all three channels of every pixel. Each channel contributes current
     * in proportion to its 0..255 value. Accumulate the raw sum first and
     * divide once at the end: dividing per pixel would throw away the
     * fractional part 72 times over and understate the total by enough to
     * matter against a 450 mA cap. */
    uint32_t sum = 0u;
    for (unsigned i = 0; i < count; ++i) {
        sum += (uint32_t)px[i].r + px[i].g + px[i].b;
    }

    const uint32_t signal_ma = (sum * LED_MA_PER_CHANNEL_FULL) / 255u;
    const uint32_t quiescent_ma = (uint32_t)count * LED_MA_QUIESCENT_PER_LED;
    return signal_ma + quiescent_ma;
}

uint16_t led_apply_cap(led_rgb_t *px, uint8_t count, uint32_t cap_ma)
{
    if (!px || count == 0u) { return 256u; }

    const uint32_t now_ma = led_estimate_ma(px, count);
    if (now_ma <= cap_ma) { return 256u; }          /* nothing to do */

    /* Quiescent current cannot be scaled away — the controllers draw it
     * whatever colour they are showing. Only the signal portion is
     * controllable, so the scale factor must be computed against that, not
     * against the total. Getting this wrong leaves the frame just over the cap
     * at high LED counts, which is the least useful place to be wrong. */
    const uint32_t quiescent_ma = (uint32_t)count * LED_MA_QUIESCENT_PER_LED;
    if (cap_ma <= quiescent_ma) {
        /* The cap cannot be met even with every LED dark. Blank the strip: it
         * is the closest achievable state, and a dark bar is an honest signal
         * that something is wrong with the configuration. */
        memset(px, 0, (size_t)count * sizeof(led_rgb_t));
        return 0u;
    }
    const uint32_t signal_now = now_ma - quiescent_ma;
    const uint32_t signal_cap = cap_ma - quiescent_ma;

    /* Scale in 1/256ths, rounding DOWN so the result is always at or under the
     * cap rather than one LSB over it. */
    uint32_t scale = (signal_cap * 256u) / signal_now;
    if (scale > 256u) { scale = 256u; }

    for (unsigned i = 0; i < count; ++i) {
        px[i].r = (uint8_t)((px[i].r * scale) >> 8);
        px[i].g = (uint8_t)((px[i].g * scale) >> 8);
        px[i].b = (uint8_t)((px[i].b * scale) >> 8);
    }
    return (uint16_t)scale;
}

#ifndef FIRMWARE_HOST_BUILD
static void led_expand(void);
void led_dma_start(void);
#endif

/* ---------------------------------------------------------------------------
 * Buffer API
 * ------------------------------------------------------------------------ */

void led_init(void)
{
    memset(s_px, 0, sizeof s_px);
    /* Strip left dark. A boot animation would be pleasant and is deliberately
     * not here: at power-on the current cap has not yet been validated against
     * a measured A3 figure, and the first thing this board should do on a car
     * is draw as little as possible until firmware is known to be running. */
}

void led_set(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= LED_TOTAL) { return; }
    s_px[index].r = r;
    s_px[index].g = g;
    s_px[index].b = b;
}

void led_fill(uint8_t r, uint8_t g, uint8_t b)
{
    for (unsigned i = 0; i < LED_TOTAL; ++i) {
        s_px[i].r = r;
        s_px[i].g = g;
        s_px[i].b = b;
    }
}

led_rgb_t *led_debug_buffer(void) { return s_px; }

void led_show(void)
{
    /* THE choke point. Every frame passes through here, and the cap is applied
     * unconditionally — there is no "trusted" caller and no bypass path.
     *
     * The cap is read fresh every frame rather than latched at init, because
     * the supply can change underneath a running board: unplug the car harness
     * from a wheel that also has USB in, and the ceiling has to come down
     * before the next frame goes out, not at the next reset. */
    (void)led_apply_cap(s_px, LED_TOTAL, power_led_cap_ma());

#ifndef FIRMWARE_HOST_BUILD
    led_dma_start();
#endif
}

/* ---------------------------------------------------------------------------
 * Bit expansion and DMA
 *
 * ⚠ ASSUMPTION A3 again, and this time it is TIMING, not just current. The
 * WS2812B-2020 datasheet is image-only, so the pulse widths below are the
 * widely used WS2812B figures rather than values read from the part's own
 * spec: T0H 0.40 µs, T1H 0.80 µs, bit period 1.25 µs, reset > 50 µs.
 *
 * The 2020 package is known to be fussier than the 5050 about reset length —
 * several vendors quote 280 µs — so the reset block below is sized generously
 * rather than to the 50 µs minimum. Cheap insurance; the cost is 280 µs of
 * idle line per refresh, which at any sane refresh rate is irrelevant.
 *
 * Symptom if the timings are wrong: the first LED lights correctly and the
 * rest show wrong or random colours, because each LED reshapes the signal it
 * passes on. Confirm on a scope at staged bring-up step 7.
 * ------------------------------------------------------------------------ */
#ifndef FIRMWARE_HOST_BUILD

#define WS2812_BIT_PERIOD_NS 1250u
#define WS2812_T0H_NS         400u
#define WS2812_T1H_NS         800u
#define WS2812_RESET_US       280u

/* Timer ticks per bit at the system clock. Computed, not hand-tuned, so a
 * change to SYSCLK_HZ cannot silently break the waveform. */
#define WS2812_TICKS_PER_BIT ((uint32_t)((uint64_t)SYSCLK_HZ * WS2812_BIT_PERIOD_NS / 1000000000u))
#define WS2812_CMP_0 ((uint16_t)((uint64_t)SYSCLK_HZ * WS2812_T0H_NS / 1000000000u))
#define WS2812_CMP_1 ((uint16_t)((uint64_t)SYSCLK_HZ * WS2812_T1H_NS / 1000000000u))

/* Trailing low bits to hold the line down for the reset period. */
#define WS2812_RESET_SLOTS ((WS2812_RESET_US * 1000u) / WS2812_BIT_PERIOD_NS + 1u)

#define WS2812_BITS   (LED_TOTAL * 24u)
#define WS2812_BUFLEN (WS2812_BITS + WS2812_RESET_SLOTS)

static uint16_t s_dma[WS2812_BUFLEN];

static void led_expand(void)
{
    unsigned k = 0;
    for (unsigned i = 0; i < LED_TOTAL; ++i) {
        /* WIRE ORDER IS GRB, not RGB, and MSB first. Getting this wrong gives
         * a strip where red and green are swapped — which looks like a
         * software colour bug and is actually a protocol one. */
        const uint8_t bytes[3] = { s_px[i].g, s_px[i].r, s_px[i].b };
        for (unsigned b = 0; b < 3u; ++b) {
            for (int bit = 7; bit >= 0; --bit) {
                s_dma[k++] = (bytes[b] & (1u << bit)) ? WS2812_CMP_1 : WS2812_CMP_0;
            }
        }
    }
    /* Reset: compare 0 holds the output low for the whole slot. */
    while (k < WS2812_BUFLEN) { s_dma[k++] = 0u; }
}

void led_dma_start(void)
{
    /* Do not restart while a transfer is in flight — a half-written frame is a
     * corrupt frame, and the strip latches whatever it received. */
    if (DMA1_Channel1->CCR & DMA_CCR_EN) { return; }

    led_expand();

    LED_DATA_TIM->PSC = 0u;
    LED_DATA_TIM->ARR = (uint16_t)(WS2812_TICKS_PER_BIT - 1u);
    LED_DATA_TIM->CCR1 = 0u;

    DMA1_Channel1->CCR   = 0u;
    DMA1_Channel1->CPAR  = (uint32_t)&LED_DATA_TIM->CCR1;
    DMA1_Channel1->CMAR  = (uint32_t)s_dma;
    DMA1_Channel1->CNDTR = WS2812_BUFLEN;
    DMA1_Channel1->CCR   = DMA_CCR_MINC | DMA_CCR_DIR
                         | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0   /* 16-bit both */
                         | DMA_CCR_TCIE | DMA_CCR_EN;

    LED_DATA_TIM->DIER |= TIM_DIER_CC1DE;     /* CC1 triggers a DMA request */
    LED_DATA_TIM->CR1  |= TIM_CR1_CEN;
}

void DMA1_Channel1_IRQHandler(void)
{
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR = DMA_IFCR_CTCIF1;
        LED_DATA_TIM->CR1  &= ~TIM_CR1_CEN;
        LED_DATA_TIM->DIER &= ~TIM_DIER_CC1DE;
        DMA1_Channel1->CCR &= ~DMA_CCR_EN;
        /* Leave the pin low. The 300 Ω series resistor and the AHCT buffer
         * hold the line at a defined level with the timer stopped. */
    }
}

#endif /* !FIRMWARE_HOST_BUILD */

#endif /* BOARD_WHEEL */
