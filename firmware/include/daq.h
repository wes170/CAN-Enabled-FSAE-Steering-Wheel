/* daq.h — eight-channel analog data acquisition on the dash.
 *
 * Signal chain per channel, from the connector inward:
 *
 *   AINn (0-5 V) --[10k]--+-- AINn_ADC --> MCU
 *                         +--[10k]-- GND        divide by 2
 *                         +--[100nF]-- GND      ~320 Hz filter
 *                         +-- BAV199 clamps to +3V3 and GND
 *
 * So the MCU sees 0-2.5 V for a 0-5 V sensor, from a 5 kΩ Thévenin source.
 * Two channels double as the ARB position feedback (AIN7, AIN8).
 */
#ifndef DAQ_H
#define DAQ_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_DASH

/*  ADC reference. VREF+ is tied to VDDA, which is +3V3 through a ferrite.
 *  ⚠ This is the rail voltage, not a precision reference — a 3.3 V regulator
 *  is typically ±2%, so every reading inherits that as a gain error. For the
 *  temperatures and pressures this front end is for, 2% is acceptable. If a
 *  channel ever needs better, the fix is ratiometric sensing (excite the
 *  sensor from the same rail and the error cancels), not a bigger number here. */
#define ADC_VREF_MV 3300u

#define ADC_FULL_SCALE 4095u

/*  A channel that has never been converted must not read as 0 V — the same
 *  "never arrived vs. genuinely zero" distinction as the CAN staleness logic.
 *  A 0 V reading on a temperature channel is a plausible sensor value, so
 *  without this a dead ADC looks like a cold engine. */
typedef struct {
    uint16_t raw;          /* last conversion, 0..4095      */
    uint16_t mv_at_pin;    /* converted, millivolts at the MCU pin */
    uint16_t mv_at_input;  /* undivided, millivolts at the connector */
    bool     valid;        /* false until the first conversion */
} daq_channel_t;

void daq_init(void);

/* Called by the DMA complete handler with the raw scan results. */
void daq_on_scan_complete(const uint16_t *raw, uint8_t count);

const daq_channel_t *daq_channel(uint8_t index);

/* ---------------------------------------------------------------------------
 * Host-testable conversions
 * ------------------------------------------------------------------------ */

/* Raw ADC code -> millivolts at the MCU pin. */
uint16_t daq_raw_to_mv(uint16_t raw);

/* Millivolts at the pin -> millivolts at the connector, undoing the /2. */
uint16_t daq_mv_at_input(uint16_t mv_at_pin);

/*  Position 0..4095 for the ARB feedback channels, expressed on the same scale
 *  the servo command uses. Deliberately the same scale on both sides so the
 *  divergence comparison in servo.c is apples to apples — a conversion
 *  mismatch there would make a healthy mechanism look permanently diverged. */
uint16_t daq_to_servo_position(uint16_t raw);

/*  The ADC channel number this DAQ index converts, from AIN_CHANNELS.
 *  Exposed so the host tests can check the table against the datasheet rather
 *  than the table only existing inside the hardware path where nothing sees
 *  it. Getting these wrong is defect 8.3, which read a plausible 14.25 V from
 *  the wrong pin and never looked broken. */
uint8_t daq_channel_number(uint8_t index);

#endif /* BOARD_DASH */
#endif /* DAQ_H */
