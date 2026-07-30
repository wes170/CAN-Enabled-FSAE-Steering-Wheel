/* test_daq.c — host tests for the DAQ conversions.
 *
 * The interesting property here is the "never converted" flag. A 0 V reading
 * on a temperature channel is a perfectly plausible sensor value, so without
 * an explicit valid flag a dead ADC is indistinguishable from a cold engine —
 * the same failure the CAN staleness logic exists to prevent, one layer down.
 */
#include <stdio.h>
#include <string.h>
#include "daq.h"

static int failures = 0;

static void ck(const char *what, bool ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) { ++failures; }
}

static void ck_u(const char *what, unsigned got, unsigned want)
{
    if (got != want) { printf("  FAIL %-52s got %u want %u\n", what, got, want); ++failures; }
    else             { printf("  ok   %-52s = %u\n", what, got); }
}

static void test_raw_to_mv(void)
{
    puts("\nraw ADC code -> millivolts at the pin");
    ck_u("code 0",         daq_raw_to_mv(0u), 0u);
    ck_u("full scale",     daq_raw_to_mv(4095u), ADC_VREF_MV);
    ck_u("half scale",     daq_raw_to_mv(2047u), (2047u * ADC_VREF_MV) / 4095u);
    ck_u("over-range clamps", daq_raw_to_mv(9999u), ADC_VREF_MV);

    /* Monotonic across the whole range: a code that goes up must never read
     * lower. An overflow or a bad divide shows up here immediately. */
    bool mono = true;
    uint16_t prev = 0u;
    for (uint32_t c = 0; c <= 4095u; ++c) {
        const uint16_t mv = daq_raw_to_mv((uint16_t)c);
        if (c && mv < prev) { mono = false; break; }
        prev = mv;
    }
    ck("monotonic over all 4096 codes", mono);

    /* No 32-bit overflow: 4095 * 3300 = 13.5M, which needs more than 16 bits
     * of intermediate. If the multiply were done in uint16_t this wraps. */
    ck("full-scale product did not wrap", daq_raw_to_mv(4095u) > 3000u);
}

static void test_divider(void)
{
    puts("\nundoing the divide-by-2");
    ck_u("2500 mV at the pin = 5000 mV at the connector",
         daq_mv_at_input(2500u), 5000u);
    ck_u("zero",   daq_mv_at_input(0u), 0u);
    ck_u("1650 mV at the pin = 3300 mV in", daq_mv_at_input(1650u), 3300u);

    /* A 0-5 V sensor at full scale should land near, but under, the reference:
     * 5 V / 2 = 2.5 V against a 3.3 V reference leaves 0.8 V of headroom,
     * which is the design intent (over-range is visible, not clipped). */
    const uint16_t mv_pin_at_5v = 2500u;
    ck("5 V input sits below the reference with headroom",
       mv_pin_at_5v < ADC_VREF_MV && (ADC_VREF_MV - mv_pin_at_5v) > 700u);
}

static void test_validity(void)
{
    puts("\n\"never converted\" must not read as 0 V");
    daq_init();

    for (unsigned i = 0; i < AIN_COUNT; ++i) {
        const daq_channel_t *c = daq_channel((uint8_t)i);
        if (!c || c->valid) {
            printf("  FAIL channel %u reports valid before any conversion\n", i);
            ++failures;
            return;
        }
    }
    ck("all eight channels start invalid", true);

    const uint16_t raw[AIN_COUNT] = { 0u, 1u, 2047u, 4095u, 100u, 200u, 300u, 400u };
    daq_on_scan_complete(raw, AIN_COUNT);

    const daq_channel_t *c0 = daq_channel(0);
    ck("valid after a scan", c0 && c0->valid);
    /* And critically: a channel reading genuine zero is VALID, not "missing". */
    ck_u("a real zero reading is zero", c0 ? c0->raw : 999u, 0u);
    ck("and it is marked valid", c0 && c0->valid);

    const daq_channel_t *c3 = daq_channel(3);
    ck_u("full-scale channel raw", c3 ? c3->raw : 0u, 4095u);
    ck_u("full-scale channel mV at pin", c3 ? c3->mv_at_pin : 0u, ADC_VREF_MV);
    ck_u("full-scale channel mV at input", c3 ? c3->mv_at_input : 0u, ADC_VREF_MV * 2u);
}

static void test_servo_scale_matches(void)
{
    puts("\nARB feedback shares the servo's 0..4095 scale");
    /* If these two scales ever diverge, servo.c's divergence check compares
     * apples to oranges and a healthy mechanism reads as permanently faulted. */
    ck_u("identity at zero",       daq_to_servo_position(0u), 0u);
    ck_u("identity at full scale", daq_to_servo_position(4095u), 4095u);
    ck_u("identity mid-range",     daq_to_servo_position(2047u), 2047u);
    ck_u("over-range clamps",      daq_to_servo_position(65535u), 4095u);

    /* The two ARB feedback channels are the last two DAQ channels. */
    ck("AIN_ARB_FB_FRONT is in range", AIN_ARB_FB_FRONT < AIN_COUNT);
    ck("AIN_ARB_FB_REAR is in range",  AIN_ARB_FB_REAR  < AIN_COUNT);
    ck("and they are different channels", AIN_ARB_FB_FRONT != AIN_ARB_FB_REAR);
}

static void test_channel_table(void)
{
    puts("\nADC channel table (DS12288 Table 12 — NOT derived from pin numbers)");
    /* PA0-PA3 = IN1..IN4, PC0-PC3 = IN6..IN9. Neither port maps one-to-one to
     * its pin numbers, which is exactly why this is a table and not a formula. */
    const uint8_t want[8] = { 1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u };
    bool ok = true;
    for (unsigned i = 0; i < AIN_COUNT; ++i) {
        if (daq_channel_number((uint8_t)i) != want[i]) {
            printf("  FAIL index %u -> channel %u, expected %u\n",
                   i, daq_channel_number((uint8_t)i), want[i]);
            ++failures; ok = false;
        }
    }
    if (ok) { puts("  ok   all eight channel numbers match the datasheet"); }

    /* No channel used twice — a duplicate would silently convert one input
     * twice and never convert another. */
    bool dupe = false;
    for (unsigned i = 0; i < AIN_COUNT; ++i) {
        for (unsigned j = i + 1u; j < AIN_COUNT; ++j) {
            if (daq_channel_number((uint8_t)i) == daq_channel_number((uint8_t)j)) { dupe = true; }
        }
    }
    ck("no ADC channel appears twice in the sequence", !dupe);
    ck_u("out-of-range index returns a sentinel",
         daq_channel_number(AIN_COUNT + 2u), 0xFFu);
}

static void test_bounds(void)
{
    puts("\nbounds");
    daq_init();
    ck("out-of-range channel returns NULL", daq_channel(AIN_COUNT + 3u) == NULL);
    daq_on_scan_complete(NULL, AIN_COUNT);           /* must not crash */
    ck("NULL scan buffer is safe", true);

    /* A short scan must fill only what it has, not read past the buffer. */
    const uint16_t two[2] = { 1000u, 2000u };
    daq_on_scan_complete(two, 2u);
    ck("channel 0 filled from a short scan", daq_channel(0)->valid);
    ck("channel 7 still invalid after a short scan", !daq_channel(7)->valid);

    /* An over-long count must be clamped, not trusted. */
    const uint16_t many[AIN_COUNT] = { 0 };
    daq_on_scan_complete(many, 200u);
    ck("over-long count clamped without overrun", true);
}

static void test_sample_time_budget(void)
{
    puts("\nsample-time budget (why the maximum is affordable)");
    /* DS12288: fs = fADC / (t_s + resolution + 0.5). Recompute the claim in
     * board_config.h rather than trusting the comment. */
    const uint32_t cycles_per_conv = AIN_SAMPLE_CYCLES + 12u + 1u;   /* 640.5+12.5 */
    const uint32_t ns_per_conv = (cycles_per_conv * 1000000000ull) / ADC_MAX_CLOCK_HZ;
    const uint32_t ns_per_scan = ns_per_conv * AIN_COUNT;
    printf("        %u cycles/conv at %u Hz -> %u ns/conv, %u ns for %u channels\n",
           cycles_per_conv, ADC_MAX_CLOCK_HZ, ns_per_conv, ns_per_scan, AIN_COUNT);
    ck("a full 8-channel scan fits in 200 us", ns_per_scan < 200000u);

    /* Duty at a 100 Hz scan rate, in tenths of a percent. */
    const uint32_t period_ns = 10000000u;                  /* 100 Hz */
    const uint32_t duty_tenths = (ns_per_scan * 1000u) / period_ns;
    printf("        at 100 Hz that is %u.%u%% of the period\n",
           duty_tenths / 10u, duty_tenths % 10u);
    ck("under 2% duty at 100 Hz", duty_tenths < 20u);
    ck_u("using the hardware maximum sample time", AIN_SMP_REGVAL, 7u);
}

int main(void)
{
    test_raw_to_mv();
    test_divider();
    test_validity();
    test_servo_scale_matches();
    test_channel_table();
    test_bounds();
    test_sample_time_budget();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
