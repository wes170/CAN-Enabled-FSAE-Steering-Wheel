/* power.c — supply detection and the LED cap that follows from it.
 *
 * Pure arithmetic and a two-state latch. No registers, no hardware calls: the
 * ADC reading arrives from daq/adc code as a number, which keeps every decision
 * in here host-testable.
 */

#include "power.h"

#ifdef BOARD_WHEEL

static power_source_t s_src = POWER_SRC_USB;   /* unknown == USB, deliberately */
static bool           s_usb_configured;

uint32_t power_v12_mv_from_adc(uint16_t adc_counts)
{
    if (adc_counts > ADC_FULL_SCALE) { adc_counts = ADC_FULL_SCALE; }

    /* Multiply before dividing, and do the divider ratio as one fraction, so
     * neither step throws away resolution. Worst case here is
     * 4095 * 3300 * 57 = 7.7e8 — comfortably inside uint32_t, but computed in
     * uint32_t explicitly rather than relying on int promotion. */
    const uint32_t pin_mv = ((uint32_t)adc_counts * ADC_VREF_MV) / ADC_FULL_SCALE;
    return (pin_mv * (V12_DIV_TOP_OHM + V12_DIV_BOTTOM_OHM)) / V12_DIV_BOTTOM_OHM;
}

power_source_t power_source_update(uint32_t v12_mv)
{
    /* Hysteresis, not a single threshold. A bare comparison at one level
     * chatters when the rail sits on it — and "chatters" here means the LED cap
     * flipping between 450 mA and 300 mA at whatever rate the ADC scans, which
     * looks like a flickering shift bar rather than like a power problem. */
    if (s_src == POWER_SRC_VEHICLE) {
        if (v12_mv < V12_ABSENT_MV)  { s_src = POWER_SRC_USB; }
    } else {
        if (v12_mv >= V12_PRESENT_MV) { s_src = POWER_SRC_VEHICLE; }
    }
    return s_src;
}

power_source_t power_source_get(void) { return s_src; }

void power_set_usb_configured(bool configured) { s_usb_configured = configured; }
bool power_usb_configured(void)                { return s_usb_configured; }

uint32_t power_led_cap_ma(void)
{
    if (s_src == POWER_SRC_VEHICLE) {
        /* The vehicle rail is present. USB enumeration state is irrelevant —
         * a car wheel with a laptop plugged in for DFU is still vehicle-powered,
         * and D6 is reverse-biased the whole time. */
        return LED_CAP_MA_VEHICLE;
    }
    return s_usb_configured ? LED_CAP_MA_USB_CONFIGURED : LED_CAP_MA_USB_DEFAULT;
}

void power_reset_state(void)
{
    s_src = POWER_SRC_USB;
    s_usb_configured = false;
}

#endif /* BOARD_WHEEL */
