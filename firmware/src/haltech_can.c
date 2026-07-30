/* haltech_can.c — the three Haltech protocols.
 *
 * Deliberately hardware-free. Everything here is state machines and byte
 * layout; the only outward dependency is the can_tx_fn hook. That is what lets
 * the whole file be exercised on a host, which matters because assumption A1
 * (the keypad node ID) is expected to be wrong on first bring-up and you want
 * to be able to rule out your own state machine before blaming the ID.
 *
 * Frame layouts: memory/system-architecture-and-can.md §2, transcribed from the
 * Haltech CAN Broadcast Protocol spec v2.0, the Blink PKP2600SI CANopen manual,
 * and the PT Motorsport IO12 emulator source.
 *
 * Byte order is BIG-ENDIAN throughout. This is a CANopen-derived automotive bus,
 * not a little-endian MCU convention; be16() exists so that never gets confused.
 */

#include "haltech_can.h"
#include <string.h>

/* --------------------------------------------------------------------------
 * Transmit hook
 * ----------------------------------------------------------------------- */
static can_tx_fn s_tx;

void haltech_set_tx(can_tx_fn fn) { s_tx = fn; }

static bool tx(uint16_t id, const uint8_t *d, uint8_t len)
{
    return s_tx ? s_tx(id, d, len) : false;
}

/* ==========================================================================
 * 1. Keypad emulation (CANopen)
 *
 * The ordering rule is the whole game here: after power-on a CANopen node
 * announces itself with a boot-up frame and then stays quiet until the master
 * sends NMT "start remote node". Transmitting key states before that is the
 * classic reason an emulated keypad is silently ignored — the ECU is not
 * listening yet, and nothing reports an error.
 * ======================================================================= */

static struct {
    keypad_state_t state;
    uint16_t       keys;          /* bit N set = key N+1 pressed, 12 keys */
    uint8_t        tick;          /* byte 4, increments every 100 ms      */
    uint32_t       ms_accum;      /* 1 ms accumulator for the 100 ms tick */
    bool           boot_sent;
    uint8_t        led_red, led_green, led_blue;   /* keys 1-8 mirrors    */
} kp;

void keypad_init(void)
{
    memset(&kp, 0, sizeof kp);
    kp.state = KEYPAD_STATE_BOOT;
}

keypad_state_t keypad_state(void) { return kp.state; }

void keypad_set_key(uint8_t key_index, bool pressed)
{
    if (key_index > 11u) { return; }          /* 12 keys, 0-based */
    const uint16_t mask = (uint16_t)(1u << key_index);
    if (pressed) { kp.keys |= mask; } else { kp.keys = (uint16_t)(kp.keys & ~mask); }
}

/* NMT frame: ID 0x000, payload { command, node_id }.
 *   command 0x01 = start remote node
 *   node_id 0    = "all nodes", which addresses us too. Handling only an exact
 *                  node-ID match is a real interoperability bug: a master that
 *                  broadcasts a global start would leave us mute forever. */
void keypad_on_nmt(const uint8_t *data, uint8_t len)
{
    if (len < 2u) { return; }
    const uint8_t command = data[0];
    const uint8_t node    = data[1];
    if (node != 0u && node != KEYPAD_NODE_ID) { return; }

    switch (command) {
    case 0x01u:  kp.state = KEYPAD_STATE_OPERATIONAL; break;  /* start        */
    case 0x02u:                                               /* stop         */
    case 0x80u:  kp.state = KEYPAD_STATE_BOOT;        break;  /* pre-op       */
    case 0x81u:                                               /* reset node   */
    case 0x82u:                                               /* reset comms  */
        keypad_init();                                        /* re-announce  */
        break;
    default: break;
    }
}

void keypad_task_1ms(void)
{
    /* Boot-up frame: ID 0x700 + node, one zero byte. Sent once, and re-sent
     * after any NMT reset because keypad_init() clears boot_sent. */
    if (!kp.boot_sent) {
        const uint8_t z = 0u;
        if (tx(CAN_ID_KEYPAD_BOOT, &z, 1u)) { kp.boot_sent = true; }
        return;                       /* nothing else until we have announced */
    }

    if (++kp.ms_accum < KEYPAD_TICK_PERIOD_MS) { return; }
    kp.ms_accum = 0u;
    kp.tick++;                        /* free-running 8-bit, wraps by design */

    /* Silence until the master says go. */
    if (kp.state != KEYPAD_STATE_OPERATIONAL) { return; }

    uint8_t f[8] = { 0 };
    f[0] = (uint8_t)(kp.keys & 0xFFu);          /* keys 1-8              */
    f[1] = (uint8_t)((kp.keys >> 8) & 0x0Fu);   /* keys 9-12, low nibble */
    f[2] = 0u;
    f[3] = 0u;
    f[4] = kp.tick;
    (void)tx(CAN_ID_KEYPAD_KEYS, f, 8u);
}

/* LED feedback from the ECU. Stored, not acted on here — the LED driver reads
 * these and paints the WS2812 haloes, so that the tuner's existing NSP LED
 * configuration drives our lights without a separate mapping to maintain. */
void keypad_on_led_frame(uint16_t can_id, const uint8_t *data, uint8_t len)
{
    if (len < 5u) { return; }
    if (can_id != CAN_ID_KEYPAD_LED_ON && can_id != CAN_ID_KEYPAD_LED_BLK) { return; }
    kp.led_red   = data[0];                          /* red   keys 8..1   */
    kp.led_green = (uint8_t)(data[1] >> 4);          /* green keys 4..1   */
    kp.led_blue  = data[3];                          /* blue  keys 8..1   */
}

void keypad_get_leds(uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (red)   { *red   = kp.led_red;   }
    if (green) { *green = kp.led_green; }
    if (blue)  { *blue  = kp.led_blue;  }
}

/* ==========================================================================
 * 2. IO12 expander emulation
 *
 * Four analog-input channels, transmitted as if we were a Haltech IO12 box.
 * The ECU sees encoder positions as voltages on AVI channels, which is exactly
 * what a real rotary trim module looks like to it — a resistor ladder into an
 * analog input. Synthesising the voltage digitally removes the ladder's
 * contact-resistance error rather than emulating it.
 *
 * The keep-alive matters: stop sending 0x2C7 and the ECU marks the box missing
 * and stops trusting the AVI values, which presents as trims that work for a
 * few seconds after boot and then quietly freeze.
 * ======================================================================= */

static struct {
    uint16_t avi[4];
    uint32_t ms_avi;
    uint32_t ms_keepalive;
} io12;

void io12_init(void) { memset(&io12, 0, sizeof io12); }

void io12_set_avi(uint8_t channel, uint16_t raw)
{
    if (channel > 3u) { return; }
    if (raw > IO12_AVI_FULL_SCALE) { raw = IO12_AVI_FULL_SCALE; }
    io12.avi[channel] = raw;
}

static void be16_put(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

void io12_task_1ms(void)
{
    if (++io12.ms_avi >= IO12_AVI_PERIOD_MS) {
        io12.ms_avi = 0u;
        uint8_t f[8];
        for (unsigned i = 0; i < 4u; ++i) {
            be16_put(&f[i * 2u], io12.avi[i]);
        }
        (void)tx(CAN_ID_IO12_AVI, f, 8u);
    }

    if (++io12.ms_keepalive >= IO12_KEEPALIVE_PERIOD_MS) {
        io12.ms_keepalive = 0u;
        static const uint8_t ka[] = IO12_KEEPALIVE_PAYLOAD;
        (void)tx(CAN_ID_IO12_KEEPALIVE, ka, (uint8_t)sizeof ka);
    }
}

/* ==========================================================================
 * 3. Broadcast decode
 * ======================================================================= */

/* Unsigned subtraction is wrap-safe: at the 49.7-day rollover of a uint32_t
 * millisecond counter, (now - last) still yields the true elapsed interval.
 * Comparing timestamps directly (now > last + timeout) would NOT be safe, and
 * would make every channel read stale for 500 ms once every seven weeks. */
bool bc_channel_is_stale(const bc_channel_t *ch, uint32_t now_ms)
{
    if (!ch->seen) { return true; }        /* never received != fresh zero */
    return (uint32_t)(now_ms - ch->last_rx_ms) >= BC_STALE_TIMEOUT_MS;
}

static void set_ch(bc_channel_t *ch, uint16_t raw, uint32_t now_ms)
{
    ch->raw        = raw;
    ch->last_rx_ms = now_ms;
    ch->seen       = true;
}

void haltech_decode(uint16_t can_id, const uint8_t *data, uint8_t len,
                    uint32_t now_ms, haltech_data_t *out)
{
    if (!out || !data) { return; }

    /* Every frame below reads 8 bytes. A short frame is either corruption or a
     * different device reusing the ID; decoding it would silently produce
     * garbage that looks like a real reading. Drop it. */
    if (len < 8u) { return; }

    switch (can_id) {
    case CAN_ID_BC_360:
        set_ch(&out->rpm,     be16(&data[0]), now_ms);
        set_ch(&out->map_kpa, be16(&data[2]), now_ms);
        set_ch(&out->tps,     be16(&data[4]), now_ms);
        break;

    case CAN_ID_BC_361:
        set_ch(&out->fuel_press, be16(&data[0]), now_ms);
        set_ch(&out->oil_press,  be16(&data[2]), now_ms);
        break;

    case CAN_ID_BC_368:
        set_ch(&out->lambda1, be16(&data[0]), now_ms);
        break;

    case CAN_ID_BC_370:
        set_ch(&out->vehicle_speed, be16(&data[0]), now_ms);
        set_ch(&out->gear,          be16(&data[2]), now_ms);
        break;

    case CAN_ID_BC_372:
        set_ch(&out->battery_v, be16(&data[0]), now_ms);
        break;

    case CAN_ID_BC_3E0:
        set_ch(&out->coolant_t, be16(&data[0]), now_ms);
        set_ch(&out->air_t,     be16(&data[2]), now_ms);
        set_ch(&out->fuel_t,    be16(&data[4]), now_ms);
        set_ch(&out->oil_t,     be16(&data[6]), now_ms);
        break;

    case CAN_ID_BC_3E1:
        set_ch(&out->trans_t, be16(&data[0]), now_ms);
        set_ch(&out->diff_t,  be16(&data[2]), now_ms);
        break;

    case CAN_ID_BC_3E4:
        set_ch(&out->status_bits, be16(&data[0]), now_ms);
        break;

    default:
        break;      /* not ours — the bus carries plenty we do not decode */
    }
}
