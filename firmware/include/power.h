/* power.h — which supply is actually feeding this board, and what that allows.
 *
 * Rev B.5 collapsed the CAR and SIM assembly variants into ONE build: every
 * part is fitted on every board, and the only difference between a car wheel
 * and a sim wheel is which cable is plugged in. `R_VBUS` — the 0 Ω link that
 * ORs USB VBUS into the +5 V rail through D6 — is now fitted on every board.
 *
 * That moves a decision from build time to run time. The LED current cap used
 * to be chosen by a -DBOARD_SIM compile flag, which is no longer meaningful:
 * the same binary on the same board can be vehicle-powered one minute and
 * bus-powered the next. So the cap has to follow the *supply*, and the supply
 * has to be measured.
 *
 * The measurement already exists in hardware: `V12_SENSE` (PA1, ADC12_IN2) is
 * a 47 k / 10 k divider off `+12V_P`. No board change was needed for this.
 *
 * ── The one rule that matters ────────────────────────────────────────────────
 * Unknown means USB. Every path that cannot prove the vehicle rail is present
 * returns the tighter cap. Being wrong in that direction dims the LEDs; being
 * wrong in the other direction browns out a host port with the board's own
 * display and CAN transceiver hanging off the same rail. This is the same
 * reasoning as the ADC sample time: choose the option that cannot fail
 * dangerously, and the residual error is a cost rather than a hazard.
 */
#ifndef POWER_H
#define POWER_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_WHEEL

typedef enum {
    POWER_SRC_USB     = 0,   /* bus-powered, or unknown — the safe assumption */
    POWER_SRC_VEHICLE = 1,   /* +12V_P confirmed present                      */
} power_source_t;

/* ---------------------------------------------------------------------------
 * Rail measurement
 *
 * V12_SENSE divider: R2 = 47 kΩ top, R3 = 10 kΩ bottom (wheel §2.4), so the pin
 * sees +12V_P × 10/57 and the rail is the reading × 5.7. At a 14.4 V charging
 * rail that is 2.53 V, comfortably inside the 3.3 V reference.
 * ------------------------------------------------------------------------ */
#define V12_DIV_TOP_OHM     47000u
#define V12_DIV_BOTTOM_OHM  10000u
/* ADC_VREF_MV and ADC_FULL_SCALE come from board_config.h -- one home. */

uint32_t power_v12_mv_from_adc(uint16_t adc_counts);

/* ---------------------------------------------------------------------------
 * Thresholds, and why they are where they are
 *
 * The interesting number is not "is 12 V present" but "can USB back-feed fool
 * us into thinking it is". It can: with `R_VBUS` fitted and USB the only
 * supply, VBUS reaches +12V_P through the buck's high-side body diode and
 * parks it at roughly 4.0 V (defect 8.15). So the present-threshold must sit
 * well above that back-feed level and well below anything a real vehicle rail
 * does — including a cranking dip.
 *
 *   ~4.0 V   USB back-feed through the buck        ── must read as USB
 *    6.0 V   release threshold (falling)
 *    7.0 V   assert threshold (rising)
 *   ~9–11 V  worst cranking dip                    ── may read as USB; harmless
 *   12–14.4 V normal vehicle rail                  ── reads as vehicle
 *
 * A cranking dip below 6 V briefly selects the USB cap. That is the correct
 * direction: the LEDs dim for the half-second the starter is engaged, on a rail
 * that is already sagging. The hysteresis band stops it chattering there.
 * ------------------------------------------------------------------------ */
#define V12_PRESENT_MV  7000u
#define V12_ABSENT_MV   6000u

/* ---------------------------------------------------------------------------
 * Caps
 *
 * Vehicle: unchanged at 450 mA — set by the thermal and connector budget, not
 * by the supply, which can deliver far more.
 *
 * USB: 300 mA, RE-DERIVED for the single build. A USB sink that has enumerated
 * with a 500 mA descriptor may draw 500 mA total, and the LEDs are not the only
 * load on +5V:
 *
 *   AP2112K → +3V3 (MCU + display), LDO so 1:1 current   110 mA
 *   TJA1051T/3 transmitting dominant                      70 mA
 *   74AHCT1G125 LED buffer                                <1 mA
 *   ────────────────────────────────────────────────────────────
 *   non-LED total                                       ~180 mA
 *   500 − 180                                          = 320 mA headroom
 *   chosen cap, keeping 20 mA in hand                    300 mA
 *
 * The old SIM cap was 350 mA and was correct at the time: the CAN transceiver
 * was DNP in that variant, so 110 + 350 = 460 mA fitted inside 500. Fitting the
 * transceiver on every board spends 70 mA that the old number did not know
 * about, and 110 + 70 + 350 = 530 mA does not fit. **Collapsing the variants
 * changed this number** — which is exactly the kind of consequence that a build
 * flag hides and a runtime measurement does not.
 *
 * Before enumeration a device may draw only 100 mA, which the non-LED load
 * alone very nearly consumes. So the cap is ZERO until the host has configured
 * us: LEDs stay dark for the first moments on a USB cable, deliberately.
 * ------------------------------------------------------------------------ */
#define LED_CAP_MA_VEHICLE        LED_CURRENT_CAP_MA_CAR
#define LED_CAP_MA_USB_CONFIGURED LED_CURRENT_CAP_MA_USB
#define LED_CAP_MA_USB_DEFAULT    0u

/* Feed a fresh V12_SENSE reading in. Applies hysteresis and returns the source
 * now in effect. Safe to call at any rate; it holds no timers. */
power_source_t power_source_update(uint32_t v12_mv);

/* The source last decided. Before the first update() this is POWER_SRC_USB. */
power_source_t power_source_get(void);

/*  Has a rail measurement ever been fed in?
 *
 *  Without this, "LEDs are dark because nobody is calling power_source_update()"
 *  and "LEDs are dark because we really are on an unenumerated USB port" are the
 *  same observable state, and the first one costs an hour at bring-up. The
 *  fail-safe default is correct either way; this just makes it diagnosable. */
bool power_source_ever_measured(void);

/* USB enumeration state, driven by the USB device stack's SET_CONFIGURATION
 * handler. Ignored entirely when the vehicle rail is present. */
void power_set_usb_configured(bool configured);
bool power_usb_configured(void);

/* THE number led_show() uses. Never cache it — the supply can change while the
 * board is running, which is the whole point of this module. */
uint32_t power_led_cap_ma(void);

/* Test-only: return to the power-on state. */
void power_reset_state(void);

#endif /* BOARD_WHEEL */
#endif /* POWER_H */
