/* daq.c — 8-channel ADC scan, DMA into a buffer, converted on completion.
 *
 * All eight channels reach ADC1 (verified against DS12288 Table 12), so one
 * scan sequence covers the set and no channel is stranded on a different ADC
 * instance needing its own sequence and its own timing.
 *
 * Channel numbers are taken from AIN_CHANNELS in board_config.h and are NOT
 * derived from pin numbers. PA0 is channel 1 and PC0 is channel 6, so neither
 * port maps one-to-one — assuming otherwise was defect 8.3 on the wheel.
 */

#include "daq.h"
#include <string.h>

#ifdef BOARD_DASH

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

static daq_channel_t s_ch[AIN_COUNT];
static const uint8_t k_channels[AIN_COUNT] = AIN_CHANNELS;

/* ---------------------------------------------------------------------------
 * Conversions — pure
 * ------------------------------------------------------------------------ */

uint16_t daq_raw_to_mv(uint16_t raw)
{
    if (raw > ADC_FULL_SCALE) { raw = ADC_FULL_SCALE; }
    /* Multiply before dividing, in 32-bit. Doing it the other way loses the
     * fractional millivolt on every channel — with a 0.806 mV LSB that is a
     * systematic downward bias, not rounding noise. */
    return (uint16_t)(((uint32_t)raw * ADC_VREF_MV) / ADC_FULL_SCALE);
}

uint16_t daq_mv_at_input(uint16_t mv_at_pin)
{
    return (uint16_t)((uint32_t)mv_at_pin * AIN_DIVIDER_NUM);
}

uint16_t daq_to_servo_position(uint16_t raw)
{
    /* Already on the 0..4095 scale the servo command uses, so this is an
     * identity with a clamp rather than a conversion. It exists as a named
     * function anyway: if the feedback sensor's range is ever trimmed to the
     * real mechanical travel, this is the single place that changes, and the
     * servo's divergence comparison stays consistent by construction. */
    return (raw > ADC_FULL_SCALE) ? ADC_FULL_SCALE : raw;
}

/* ---------------------------------------------------------------------------
 * Scan handling
 * ------------------------------------------------------------------------ */

void daq_init(void)
{
    memset(s_ch, 0, sizeof s_ch);
    /* valid = false on every channel. Nothing reports a reading until a
     * conversion has actually happened. */
}

void daq_on_scan_complete(const uint16_t *raw, uint8_t count)
{
    if (!raw) { return; }
    if (count > AIN_COUNT) { count = AIN_COUNT; }

    for (unsigned i = 0; i < count; ++i) {
        s_ch[i].raw         = raw[i];
        s_ch[i].mv_at_pin   = daq_raw_to_mv(raw[i]);
        s_ch[i].mv_at_input = daq_mv_at_input(s_ch[i].mv_at_pin);
        s_ch[i].valid       = true;
    }
}

uint8_t daq_channel_number(uint8_t index)
{
    return (index < AIN_COUNT) ? k_channels[index] : 0xFFu;
}

const daq_channel_t *daq_channel(uint8_t index)
{
    return (index < AIN_COUNT) ? &s_ch[index] : (const daq_channel_t *)0;
}

/* ---------------------------------------------------------------------------
 * Hardware
 * ------------------------------------------------------------------------ */
#ifndef FIRMWARE_HOST_BUILD

static uint16_t s_dma_raw[AIN_COUNT];

void daq_hw_init(void)
{
    /* Sample time: the maximum the hardware offers, on every channel.
     * See the long note at AIN_SMP_REGVAL in board_config.h — this converts an
     * unresolved source-impedance question into a non-question at a cost of
     * 1% of the scan budget. SMPR1 holds channels 0-9, three bits each. */
    uint32_t smpr1 = 0u;
    for (unsigned i = 0; i < AIN_COUNT; ++i) {
        const uint8_t ch = k_channels[i];
        if (ch <= 9u) { smpr1 |= ((uint32_t)AIN_SMP_REGVAL << (ch * 3u)); }
    }
    ADC1->SMPR1 = smpr1;

    /* Regular sequence: length then the channel list. SQR1 holds L and the
     * first four conversions; SQR2 the next four. */
    ADC1->SQR1 = (uint32_t)(AIN_COUNT - 1u)
               | ((uint32_t)k_channels[0] << 6)
               | ((uint32_t)k_channels[1] << 12)
               | ((uint32_t)k_channels[2] << 18)
               | ((uint32_t)k_channels[3] << 24);
    ADC1->SQR2 = ((uint32_t)k_channels[4] << 0)
               | ((uint32_t)k_channels[5] << 6)
               | ((uint32_t)k_channels[6] << 12)
               | ((uint32_t)k_channels[7] << 18);

    ADC1->CFGR = ADC_CFGR_DMAEN | ADC_CFGR_DMACFG;   /* circular DMA */
}

void daq_start_scan(void)
{
    ADC1->CR |= ADC_CR_ADSTART;
}

void daq_dma_complete(void)
{
    daq_on_scan_complete(s_dma_raw, AIN_COUNT);
}

#endif /* !FIRMWARE_HOST_BUILD */
#endif /* BOARD_DASH */
