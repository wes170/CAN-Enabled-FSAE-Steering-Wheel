/* test_power.c — supply detection and the LED cap that follows from it.
 *
 * The property worth testing here is not "does it return 450 or 300". It is
 * "can it ever return the LARGER cap when it should not". Every failure that
 * matters is in that direction: an over-cap on a USB port browns out the host,
 * an under-cap dims some LEDs.
 */
#include <stdio.h>
#include <stdlib.h>
#include "power.h"
#include "led.h"

static int fails;
static void ck(const char *what, int cond)
{
    printf("  %-58s %s\n", what, cond ? "ok" : "FAIL");
    if (!cond) { fails++; }
}

int main(void)
{
    printf("== power source / LED cap ==\n");

    /* --- ADC conversion, against hand arithmetic ------------------------- */
    /* 12.0 V through 47k/10k = 2.105 V at the pin = 2612 counts of 4095. */
    uint32_t mv = power_v12_mv_from_adc(2612u);
    ck("12 V rail reads back within 100 mV", mv > 11900u && mv < 12100u);

    mv = power_v12_mv_from_adc(0u);
    ck("zero counts is 0 mV", mv == 0u);

    /* Full scale is 3.3 V at the pin = 18.81 V at the rail. Nothing should
     * overflow or wrap on the way. */
    mv = power_v12_mv_from_adc(ADC_FULL_SCALE);
    ck("full scale is ~18.8 V, no overflow", mv > 18700u && mv < 18900u);
    ck("out-of-range counts are clamped, not wrapped",
       power_v12_mv_from_adc(60000u) == power_v12_mv_from_adc(ADC_FULL_SCALE));

    /* --- The state that matters at power-on ----------------------------- */
    power_reset_state();
    ck("before any measurement the source is USB",
       power_source_get() == POWER_SRC_USB);
    ck("and 'never measured' is distinguishable from 'measured, it is USB'",
       !power_source_ever_measured());
    power_source_update(0u);
    ck("one reading of 0 V still says USB, but now it HAS been measured",
       power_source_get() == POWER_SRC_USB && power_source_ever_measured());
    power_reset_state();
    ck("reset clears the measured flag too", !power_source_ever_measured());
    ck("before enumeration the cap is ZERO, not the USB cap",
       power_led_cap_ma() == 0u);

    /* --- The back-feed case. THIS is the test this module exists for. ----
     * With R_VBUS fitted, USB reaches +12V_P through the buck's high-side body
     * diode and parks it near 4 V (defect 8.15). If that reads as "vehicle
     * present", a 500 mA port gets asked for 450 mA of LEDs on top of 180 mA of
     * everything else. Check the whole plausible back-feed band, not one
     * convenient point. */
    for (uint32_t v = 3000u; v <= 5500u; v += 100u) {
        power_reset_state();
        power_source_update(v);
        if (power_source_get() != POWER_SRC_USB) {
            printf("  back-feed at %u mV read as VEHICLE\n", v);
            fails++;
            break;
        }
    }
    ck("no voltage in the 3.0-5.5 V back-feed band reads as vehicle", 1);

    /* --- Normal vehicle rail -------------------------------------------- */
    power_reset_state();
    power_source_update(12000u);
    ck("12 V reads as vehicle", power_source_get() == POWER_SRC_VEHICLE);
    ck("vehicle cap is 450 mA", power_led_cap_ma() == LED_CURRENT_CAP_MA_CAR);

    power_reset_state();
    power_source_update(14400u);
    ck("14.4 V charging rail reads as vehicle",
       power_source_get() == POWER_SRC_VEHICLE);

    /* --- Hysteresis ------------------------------------------------------ */
    power_reset_state();
    power_source_update(12000u);
    power_source_update(6500u);      /* between the two thresholds */
    ck("a dip into the hysteresis band HOLDS vehicle",
       power_source_get() == POWER_SRC_VEHICLE);
    power_source_update(5500u);
    ck("below the release threshold it drops to USB",
       power_source_get() == POWER_SRC_USB);
    power_source_update(6500u);
    ck("coming back up, the band does NOT re-assert vehicle",
       power_source_get() == POWER_SRC_USB);
    power_source_update(7000u);
    ck("at the assert threshold it becomes vehicle again",
       power_source_get() == POWER_SRC_VEHICLE);

    /* Chatter: walk the band repeatedly and confirm it does not oscillate. */
    power_reset_state();
    power_source_update(12000u);
    for (int i = 0; i < 50; ++i) {
        power_source_update(6200u);
        power_source_update(6800u);
    }
    ck("50 cycles inside the band produce no state change",
       power_source_get() == POWER_SRC_VEHICLE);

    /* --- USB enumeration gating ----------------------------------------- */
    power_reset_state();
    power_source_update(0u);
    ck("USB, unconfigured: cap is zero", power_led_cap_ma() == 0u);
    power_set_usb_configured(true);
    ck("USB, configured: cap is the USB cap",
       power_led_cap_ma() == LED_CURRENT_CAP_MA_USB);
    ck("the USB cap is 300 mA", LED_CURRENT_CAP_MA_USB == 300u);

    /* Enumeration state must not leak into the vehicle case in either
     * direction -- a car wheel with a laptop plugged in for DFU is still
     * vehicle-powered, and one that has never enumerated is too. */
    power_source_update(13000u);
    ck("vehicle + configured is still the vehicle cap",
       power_led_cap_ma() == LED_CURRENT_CAP_MA_CAR);
    power_set_usb_configured(false);
    ck("vehicle + unconfigured is still the vehicle cap",
       power_led_cap_ma() == LED_CURRENT_CAP_MA_CAR);

    /* --- The budget the USB cap was derived from ------------------------- */
    /* If someone raises the cap without redoing the sum, this catches it.
     * 110 mA (3V3 via the LDO) + 70 mA (TJA1051 dominant) = 180 mA of non-LED
     * load, inside a 500 mA allowance. */
    const uint32_t non_led_ma = 110u + 70u;
    ck("USB cap + non-LED load fits inside 500 mA",
       LED_CURRENT_CAP_MA_USB + non_led_ma <= 500u);
    ck("the OLD 350 mA sim cap would NOT fit once CAN is fitted",
       350u + non_led_ma > 500u);

    /* --- Ordering ------------------------------------------------------- */
    ck("the USB cap is strictly tighter than the vehicle cap",
       LED_CURRENT_CAP_MA_USB < LED_CURRENT_CAP_MA_CAR);

    printf(fails ? "\nFAILED (%d)\n" : "\nPASSED (%d failures)\n", fails);
    return fails ? 1 : 0;
}
