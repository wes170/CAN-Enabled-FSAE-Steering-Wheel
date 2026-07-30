/* test_haltech_can.c — host tests for the three Haltech protocols.
 *
 * This file is worth more than most of the firmware, because every failure it
 * checks for is SILENT on real hardware: a keypad that transmits too early is
 * simply ignored, a byte-order error produces plausible numbers, and a stale
 * reading looks exactly like a live one. None of these announce themselves.
 */
#include <stdio.h>
#include <string.h>
#include "haltech_can.h"

static int failures = 0;

static void ck(const char *what, bool ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) { ++failures; }
}

static void ck_u(const char *what, unsigned got, unsigned want)
{
    if (got != want) { printf("  FAIL %-56s got %u want %u\n", what, got, want); ++failures; }
    else             { printf("  ok   %-56s = %u\n", what, got); }
}

/* --- capture transmitted frames ---------------------------------------- */
#define CAP_MAX 64
static struct { uint16_t id; uint8_t d[8]; uint8_t len; } cap[CAP_MAX];
static int cap_n;
static bool cap_tx(uint16_t id, const uint8_t *d, uint8_t len)
{
    if (cap_n >= CAP_MAX) { return false; }
    cap[cap_n].id = id; cap[cap_n].len = len;
    memcpy(cap[cap_n].d, d, len > 8u ? 8u : len);
    ++cap_n;
    return true;
}
static void cap_reset(void) { cap_n = 0; }
static int cap_count(uint16_t id)
{
    int n = 0;
    for (int i = 0; i < cap_n; ++i) { if (cap[i].id == id) { ++n; } }
    return n;
}
static int cap_find(uint16_t id)
{
    for (int i = 0; i < cap_n; ++i) { if (cap[i].id == id) { return i; } }
    return -1;
}

static void run_ms(unsigned ms, void (*task)(void))
{
    for (unsigned i = 0; i < ms; ++i) { task(); }
}

/* ======================================================================== */
static void test_keypad_ordering(void)
{
    puts("\nkeypad: CANopen start-up ordering (the reason emulated keypads get ignored)");
    haltech_set_tx(cap_tx);
    cap_reset();
    keypad_init();

    ck("starts in BOOT state", keypad_state() == KEYPAD_STATE_BOOT);

    keypad_task_1ms();
    ck_u("boot-up frame sent exactly once", cap_count(CAN_ID_KEYPAD_BOOT), 1u);
    ck_u("boot-up frame ID is 0x700 + node", CAN_ID_KEYPAD_BOOT, 0x715u);

    /* Half a second of ticking with no NMT: must stay silent on key states. */
    cap_reset();
    keypad_set_key(0, true);
    run_ms(500, keypad_task_1ms);
    ck_u("NO key frames before NMT start", cap_count(CAN_ID_KEYPAD_KEYS), 0u);

    /* NMT start addressed to us. */
    const uint8_t nmt_start[2] = { 0x01u, KEYPAD_NODE_ID };
    keypad_on_nmt(nmt_start, 2u);
    ck("NMT start -> OPERATIONAL", keypad_state() == KEYPAD_STATE_OPERATIONAL);

    cap_reset();
    run_ms(1000, keypad_task_1ms);
    ck_u("key frames at 100 ms -> 10 in one second", cap_count(CAN_ID_KEYPAD_KEYS), 10u);
}

static void test_keypad_nmt_variants(void)
{
    puts("\nkeypad: NMT command handling");
    haltech_set_tx(cap_tx);

    /* Global start (node 0) must address us too. A master that broadcasts a
     * global start would otherwise leave us mute forever. */
    cap_reset(); keypad_init(); keypad_task_1ms();
    const uint8_t global_start[2] = { 0x01u, 0x00u };
    keypad_on_nmt(global_start, 2u);
    ck("NMT start to node 0 (all nodes) starts us",
       keypad_state() == KEYPAD_STATE_OPERATIONAL);

    /* A start addressed to a DIFFERENT node must not start us. */
    cap_reset(); keypad_init(); keypad_task_1ms();
    const uint8_t other[2] = { 0x01u, (uint8_t)(KEYPAD_NODE_ID + 1u) };
    keypad_on_nmt(other, 2u);
    ck("NMT start for another node is ignored",
       keypad_state() == KEYPAD_STATE_BOOT);

    /* Stop returns us to silence. */
    keypad_on_nmt(global_start, 2u);
    const uint8_t stop[2] = { 0x02u, KEYPAD_NODE_ID };
    keypad_on_nmt(stop, 2u);
    ck("NMT stop -> back to BOOT", keypad_state() == KEYPAD_STATE_BOOT);
    cap_reset();
    run_ms(300, keypad_task_1ms);
    ck_u("silent again after stop", cap_count(CAN_ID_KEYPAD_KEYS), 0u);

    /* Reset must re-announce with a fresh boot-up frame. */
    const uint8_t reset[2] = { 0x81u, KEYPAD_NODE_ID };
    keypad_on_nmt(reset, 2u);
    cap_reset();
    keypad_task_1ms();
    ck_u("NMT reset re-sends the boot-up frame", cap_count(CAN_ID_KEYPAD_BOOT), 1u);

    /* Malformed frames must not crash or change state. */
    keypad_on_nmt(global_start, 1u);   /* too short */
    ck("1-byte NMT frame ignored", keypad_state() == KEYPAD_STATE_BOOT);
}

static void test_keypad_key_bits(void)
{
    puts("\nkeypad: key bit packing");
    haltech_set_tx(cap_tx);
    keypad_init(); keypad_task_1ms();
    const uint8_t start[2] = { 0x01u, KEYPAD_NODE_ID };
    keypad_on_nmt(start, 2u);

    for (uint8_t k = 0; k < 12u; ++k) { keypad_set_key(k, false); }
    keypad_set_key(0, true);    /* key 1  -> byte0 bit0 */
    keypad_set_key(7, true);    /* key 8  -> byte0 bit7 */
    keypad_set_key(8, true);    /* key 9  -> byte1 bit0 */
    keypad_set_key(11, true);   /* key 12 -> byte1 bit3 */

    cap_reset();
    run_ms(100, keypad_task_1ms);
    int i = cap_find(CAN_ID_KEYPAD_KEYS);
    ck("a key frame was sent", i >= 0);
    if (i >= 0) {
        ck_u("byte0 = keys 1-8",            cap[i].d[0], 0x81u);
        ck_u("byte1 = keys 9-12 low nibble", cap[i].d[1], 0x09u);
        ck_u("DLC is 8",                     cap[i].len,  8u);
    }

    /* Out-of-range index must be ignored, not wrap into another key. */
    keypad_set_key(12, true);
    keypad_set_key(255, true);
    cap_reset();
    run_ms(100, keypad_task_1ms);
    i = cap_find(CAN_ID_KEYPAD_KEYS);
    if (i >= 0) {
        ck_u("out-of-range key index changed nothing (byte0)", cap[i].d[0], 0x81u);
        ck_u("out-of-range key index changed nothing (byte1)", cap[i].d[1], 0x09u);
    }
}

static void test_io12(void)
{
    puts("\nIO12: AVI framing and keep-alive");
    haltech_set_tx(cap_tx);
    io12_init();
    cap_reset();

    io12_set_avi(0, 0u);
    io12_set_avi(1, 2048u);
    io12_set_avi(2, IO12_AVI_FULL_SCALE);
    io12_set_avi(3, 1u);

    run_ms(IO12_AVI_PERIOD_MS, io12_task_1ms);
    int i = cap_find(CAN_ID_IO12_AVI);
    ck("AVI frame sent after 20 ms", i >= 0);
    if (i >= 0) {
        /* BIG-endian. A little-endian slip here yields plausible-looking
         * numbers, which is exactly why it is asserted rather than eyeballed. */
        ck_u("ch1 high byte", cap[i].d[2], 0x08u);
        ck_u("ch1 low byte",  cap[i].d[3], 0x00u);
        ck_u("ch2 high byte", cap[i].d[4], 0x0Fu);
        ck_u("ch2 low byte",  cap[i].d[5], 0xFFu);
        ck_u("ch3 low byte",  cap[i].d[7], 0x01u);
    }

    /* Clamping: values above full scale must not wrap to near-zero. */
    io12_set_avi(0, 9999u);
    cap_reset();
    run_ms(IO12_AVI_PERIOD_MS, io12_task_1ms);
    i = cap_find(CAN_ID_IO12_AVI);
    if (i >= 0) {
        unsigned v = (unsigned)((cap[i].d[0] << 8) | cap[i].d[1]);
        ck_u("over-range AVI clamps to full scale", v, IO12_AVI_FULL_SCALE);
    }

    /* Rates: in one second, 50 AVI frames and 10 keep-alives. Losing the
     * keep-alive makes the ECU drop the box, which presents as trims that
     * work briefly after boot and then freeze. */
    io12_init(); cap_reset();
    run_ms(1000, io12_task_1ms);
    ck_u("AVI frames per second",       (unsigned)cap_count(CAN_ID_IO12_AVI), 50u);
    ck_u("keep-alive frames per second",(unsigned)cap_count(CAN_ID_IO12_KEEPALIVE), 10u);
}

static void test_position_mapping(void)
{
    puts("\nIO12: encoder position -> AVI code");
    ck_u("position 0 of 12",   io12_position_to_avi(0, 12), 0u);
    ck_u("position 11 of 12",  io12_position_to_avi(11, 12), IO12_AVI_FULL_SCALE);
    ck_u("midpoint 6 of 13",   io12_position_to_avi(6, 13), IO12_AVI_FULL_SCALE / 2u);
    ck_u("single detent",      io12_position_to_avi(0, 1), 0u);
    ck_u("zero detents",       io12_position_to_avi(0, 0), 0u);
    ck_u("over-range clamps",  io12_position_to_avi(50, 12), IO12_AVI_FULL_SCALE);
    /* Monotonic: a knob must never read lower when turned up. */
    bool mono = true;
    uint16_t prev = 0;
    for (uint8_t p = 0; p < 24u; ++p) {
        uint16_t v = io12_position_to_avi(p, 24);
        if (p && v <= prev) { mono = false; }
        prev = v;
    }
    ck("strictly increasing across 24 detents", mono);
}

static void test_broadcast_decode(void)
{
    puts("\nbroadcast: decode and unit conversion");
    haltech_data_t d;
    memset(&d, 0, sizeof d);

    /* 0x360: RPM=6000, MAP=1013 (101.3 kPa abs), TPS=5000 */
    const uint8_t f360[8] = { 0x17, 0x70, 0x03, 0xF5, 0x13, 0x88, 0x00, 0x00 };
    haltech_decode(CAN_ID_BC_360, f360, 8u, 1000u, &d);
    ck_u("RPM decoded big-endian", d.rpm.raw, 6000u);
    ck_u("MAP decoded",            d.map_kpa.raw, 1013u);
    ck_u("TPS decoded",            d.tps.raw, 5000u);
    ck("MAP 1013 -> ~0 kPa gauge",
       bc_press_to_kpa_gauge(d.map_kpa.raw) > -0.05f &&
       bc_press_to_kpa_gauge(d.map_kpa.raw) <  0.05f);

    /* 0x3E0: temps in 0.1 K. 3731 = 373.1 K = 99.95 C */
    const uint8_t f3E0[8] = { 0x0E, 0x93, 0x0E, 0x93, 0x0E, 0x93, 0x0E, 0x93 };
    haltech_decode(CAN_ID_BC_3E0, f3E0, 8u, 1000u, &d);
    ck_u("coolant raw", d.coolant_t.raw, 3731u);
    float c = bc_temp_to_celsius(d.coolant_t.raw);
    ck("3731 (0.1 K) -> 99.95 C", c > 99.9f && c < 100.0f);
    /* 0 K would be -273.15 C: the value the dash would show for an absent
     * sensor if staleness were not tracked. */
    ck("raw 0 -> -273.15 C, which is why staleness matters",
       bc_temp_to_celsius(0u) < -273.0f);

    /* A short frame must be dropped, not partially decoded. */
    haltech_data_t before = d;
    const uint8_t shortf[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    haltech_decode(CAN_ID_BC_360, shortf, 4u, 2000u, &d);
    ck("short frame ignored (no partial decode)", d.rpm.raw == before.rpm.raw);

    /* An unknown ID must not disturb anything. */
    haltech_decode(0x123u, f360, 8u, 2000u, &d);
    ck("unrelated CAN ID ignored", d.rpm.raw == before.rpm.raw);
}

static void test_staleness(void)
{
    puts("\nbroadcast: staleness (a frozen gauge reads as healthy)");
    haltech_data_t d;
    memset(&d, 0, sizeof d);

    /* THE case this exists for: at power-on, now_ms is small and last_rx_ms is
     * 0, so a naive elapsed-time check reports every channel fresh and the
     * dash shows 0 rpm / -273 C as live readings. */
    ck("never-received channel is stale at t=0",   bc_channel_is_stale(&d.rpm, 0u));
    ck("never-received channel is stale at t=100", bc_channel_is_stale(&d.rpm, 100u));
    ck("never-received channel is stale at t=499", bc_channel_is_stale(&d.rpm, 499u));

    const uint8_t f[8] = { 0x17, 0x70, 0, 0, 0, 0, 0, 0 };
    haltech_decode(CAN_ID_BC_360, f, 8u, 10000u, &d);
    ck("fresh immediately after rx",      !bc_channel_is_stale(&d.rpm, 10000u));
    ck("fresh at 499 ms",                 !bc_channel_is_stale(&d.rpm, 10499u));
    ck("STALE at exactly 500 ms",          bc_channel_is_stale(&d.rpm, 10500u));
    ck("stale well after",                 bc_channel_is_stale(&d.rpm, 20000u));

    /* A channel that legitimately reads zero is NOT stale. */
    haltech_data_t z;
    memset(&z, 0, sizeof z);
    const uint8_t zf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    haltech_decode(CAN_ID_BC_360, zf, 8u, 5000u, &z);
    ck("a real zero reading is fresh, not stale", !bc_channel_is_stale(&z.rpm, 5000u));
    ck_u("and its value is zero", z.rpm.raw, 0u);

    /* Millisecond counter rollover at ~49.7 days. Unsigned subtraction stays
     * correct across the wrap; comparing timestamps directly would not. */
    haltech_data_t w;
    memset(&w, 0, sizeof w);
    const uint32_t near_wrap = 0xFFFFFF00u;
    haltech_decode(CAN_ID_BC_360, f, 8u, near_wrap, &w);
    ck("fresh 100 ms after rx, across the uint32 wrap",
       !bc_channel_is_stale(&w.rpm, (uint32_t)(near_wrap + 100u)));
    ck("stale 600 ms after rx, across the uint32 wrap",
       bc_channel_is_stale(&w.rpm, (uint32_t)(near_wrap + 600u)));
}

int main(void)
{
    test_keypad_ordering();
    test_keypad_nmt_variants();
    test_keypad_key_bits();
    test_io12();
    test_position_mapping();
    test_broadcast_decode();
    test_staleness();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
