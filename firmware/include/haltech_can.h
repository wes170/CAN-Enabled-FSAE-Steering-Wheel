/*
 * haltech_can.h — Haltech Nexus R5 CAN interface.
 *
 * Three protocols live here, all on one 1 Mbit/s bus with 11-bit IDs and
 * BIG-ENDIAN payloads:
 *
 *   1. CANopen keypad emulation   — buttons -> ECU, LED commands ECU -> us.
 *   2. IO12 expander emulation    — encoder positions as synthetic analog
 *                                   voltages the ECU already knows how to map.
 *   3. Broadcast protocol decode  — engine data ECU -> us (receive only).
 *
 * Frame layouts transcribed from:
 *   Blink PKP2600SI CANopen manual rev 1.5      (keypad)
 *   PT Motorsport open-source IObox emulator    (IO12 Box A)
 *   Haltech CAN Broadcast Protocol spec V2      (broadcast)
 * See memory/system-architecture-and-can.md §2 for the evidence trail.
 */
#ifndef HALTECH_CAN_H
#define HALTECH_CAN_H

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * 1. Keypad emulation (CANopen)
 *
 * The ECU discovers us as a Haltech CAN keypad, so buttons map to functions
 * in NSP with no custom CAN configuration by the tuner.
 * ========================================================================= */

/*  Node ID. 0x15 is the Blink factory default and is what MaxxECU documents.
 *  ⚠ ASSUMPTION A1 — Haltech-branded units ship reconfigured, and we have not
 *  yet sniffed a real one. This is the single most likely thing to be wrong
 *  on first bring-up, which is why it is a constant and not baked into logic.
 *  Close it with a ~$30 USB-CAN sniffer before trusting the design. */
#define KEYPAD_NODE_ID        0x15u

#define CAN_ID_NMT            0x000u                      /* ECU -> all      */
#define CAN_ID_KEYPAD_BOOT    (0x700u + KEYPAD_NODE_ID)   /* us -> ECU       */
#define CAN_ID_KEYPAD_KEYS    (0x180u + KEYPAD_NODE_ID)   /* us -> ECU       */
#define CAN_ID_KEYPAD_LED_ON  (0x200u + KEYPAD_NODE_ID)   /* ECU -> us       */
#define CAN_ID_KEYPAD_LED_BLK (0x300u + KEYPAD_NODE_ID)   /* ECU -> us       */
#define CAN_ID_KEYPAD_BRIGHT  (0x400u + KEYPAD_NODE_ID)   /* ECU -> us       */
#define CAN_ID_KEYPAD_BACKLT  (0x500u + KEYPAD_NODE_ID)   /* ECU -> us       */

#define KEYPAD_TICK_PERIOD_MS 100u   /* byte 4 counter increments every 100ms */

/*  The keypad may NOT transmit key states until the ECU sends NMT start
 *  (ID 0x000, payload {0x01, node_id}). Sending early is the classic reason
 *  an emulated keypad is ignored. */
typedef enum {
    KEYPAD_STATE_BOOT = 0,   /* sent boot-up, waiting for NMT start */
    KEYPAD_STATE_OPERATIONAL /* NMT start received, may transmit    */
} keypad_state_t;

/* Key states: byte0 = keys 1-8, byte1 = keys 9-12 in the low nibble,
 * bytes 2,3 unused, byte4 = 100 ms tick counter. */
void keypad_init(void);
void keypad_on_nmt(const uint8_t *data, uint8_t len);
void keypad_set_key(uint8_t key_index, bool pressed);   /* 0-based, 0..11 */
void keypad_task_1ms(void);
keypad_state_t keypad_state(void);

/*  LED feedback from the ECU. Bit layout, per the Blink manual:
 *    byte0 = red   keys 8..1
 *    byte1 = green keys 4..1 (high nibble) + red keys 12..9 (low nibble)
 *    byte2 = green keys 12..5
 *    byte3 = blue  keys 8..1
 *    byte4 = blue  keys 12..9 (low nibble)
 *  We map these onto the WS2812 positions that halo the physical buttons, so
 *  the tuner's existing NSP LED configuration just works. */
void keypad_on_led_frame(uint16_t can_id, const uint8_t *data, uint8_t len);

/* Latest LED state the ECU asked for (keys 1-8 for red/blue, 1-4 for green).
 * The LED driver reads this; keypad_on_led_frame() only stores it. */
void keypad_get_leds(uint8_t *red, uint8_t *green, uint8_t *blue);

/* ===========================================================================
 * 2. IO12 expander emulation — encoder positions as AVI voltages
 *
 * A Haltech rotary trim module is electrically just a resistor ladder into an
 * analog input. So to the ECU, an absolute knob position IS a voltage. We
 * synthesise that voltage digitally, with none of the contact-resistance
 * error a real ladder has.
 * ========================================================================= */

#define CAN_ID_IO12_AVI       0x2C1u   /* us -> ECU, 20 ms, 4x u16 BE       */
#define CAN_ID_IO12_DPI12     0x2C3u   /* us -> ECU, 20 ms                  */
#define CAN_ID_IO12_DPI34     0x2C5u   /* us -> ECU, 20 ms                  */
#define CAN_ID_IO12_KEEPALIVE 0x2C7u   /* us -> ECU, 100 ms                 */

#define IO12_AVI_PERIOD_MS       20u
#define IO12_KEEPALIVE_PERIOD_MS 100u
#define IO12_AVI_FULL_SCALE      4095u  /* 0..4095 == 0..5 V                */
#define IO12_KEEPALIVE_PAYLOAD   { 0x10, 0x09, 0x0A, 0x00, 0x00 }

/*  Map a detented encoder position to the AVI code the ECU will read.
 *  position 0..(detents-1)  ->  0..4095 spread evenly.
 *  Deliberately integer maths: the ECU quantises anyway, and float here
 *  would only invite rounding differences between builds. */
static inline uint16_t io12_position_to_avi(uint8_t position, uint8_t detents)
{
    if (detents <= 1u) { return 0u; }
    if (position >= detents) { position = (uint8_t)(detents - 1u); }
    return (uint16_t)((position * IO12_AVI_FULL_SCALE) / (detents - 1u));
}

void io12_init(void);
void io12_set_avi(uint8_t channel, uint16_t raw);   /* channel 0..3 */
void io12_task_1ms(void);

/*  ⚠ ASSUMPTION A2 — only Box A (the IDs above) is verified, from the PT
 *  Motorsport emulator source. Box B's IDs are NOT publicly documented. The
 *  dash needs Box B to re-broadcast its DAQ channels. Get the write protocol
 *  from Haltech support (they supply it to owners) before implementing it. */

/* ===========================================================================
 * 3. Broadcast decode — engine data from the ECU
 *
 * Receive only. Temperatures are 0.1 K, pressures 0.1 kPa ABSOLUTE.
 * ========================================================================= */

#define CAN_ID_BC_360  0x360u  /* RPM, MAP, TPS                    50 Hz */
#define CAN_ID_BC_361  0x361u  /* fuel pressure, oil pressure      50 Hz */
#define CAN_ID_BC_368  0x368u  /* lambda 1                         20 Hz */
#define CAN_ID_BC_36C  0x36Cu  /* wheel speeds FL/FR/RL/RR         20 Hz */
#define CAN_ID_BC_370  0x370u  /* vehicle speed, GEAR              20 Hz */
#define CAN_ID_BC_372  0x372u  /* battery voltage                  10 Hz */
#define CAN_ID_BC_3E0  0x3E0u  /* coolant, air, fuel, oil temps     5 Hz */
#define CAN_ID_BC_3E1  0x3E1u  /* trans temp, diff temp             5 Hz */
#define CAN_ID_BC_3E4  0x3E4u  /* status bits                       5 Hz */

/*  Unit conversions. Get these wrong and a gauge lies quietly, which is
 *  worse than a gauge that is obviously broken. */
static inline float bc_temp_to_celsius(uint16_t raw)  { return (raw * 0.1f) - 273.15f; }
static inline float bc_press_to_kpa_gauge(uint16_t raw){ return (raw * 0.1f) - 101.3f;  }
static inline float bc_volts(uint16_t raw)            { return raw * 0.1f;              }
static inline float bc_lambda(uint16_t raw)           { return raw * 0.001f;            }

/*  Every decoded channel carries a timestamp so the UI can tell "0" from
 *  "no data". A frozen coolant reading looks fine while the engine cooks —
 *  showing dashes after 500 ms of silence is a REQUIREMENT, not a nicety. */
#define BC_STALE_TIMEOUT_MS 500u

typedef struct {
    uint16_t raw;
    uint32_t last_rx_ms;
    bool     seen;        /* false until the first frame carrying this channel */
} bc_channel_t;

/*  `seen` is not redundant with `last_rx_ms`. Without it, a channel that has
 *  NEVER been received reads as last_rx_ms = 0, and at power-on now_ms is also
 *  near 0 — so (now - last) < 500 ms and the channel reports FRESH. The dash
 *  would then display 0 rpm, 0 kPa and −273 °C as though they were live
 *  measurements during the first half second after boot, which is precisely
 *  the failure the stale-data requirement exists to prevent. "Never arrived"
 *  and "arrived, reading zero" must be distinguishable. */

typedef struct {
    bc_channel_t rpm, map_kpa, tps;
    bc_channel_t fuel_press, oil_press;
    bc_channel_t lambda1;
    bc_channel_t vehicle_speed, gear;
    bc_channel_t battery_v;
    bc_channel_t coolant_t, air_t, fuel_t, oil_t;
    bc_channel_t trans_t, diff_t;
    bc_channel_t status_bits;
} haltech_data_t;

bool bc_channel_is_stale(const bc_channel_t *ch, uint32_t now_ms);
void haltech_decode(uint16_t can_id, const uint8_t *data, uint8_t len,
                    uint32_t now_ms, haltech_data_t *out);

/* ===========================================================================
 * 4. Transmit hook
 *
 * The protocol modules never touch FDCAN registers. They call this, and the
 * board layer supplies it. That keeps every state machine in this file
 * testable on a host with no hardware — which matters, because assumption A1
 * (the keypad node ID) means the first bring-up attempt is expected to fail
 * and the question will be "is my state machine wrong or is the ID wrong?".
 * Being able to answer the first half at a desk narrows that considerably.
 *
 * Return false if the frame could not be queued; callers treat that as
 * non-fatal and retry on the next tick.
 * ========================================================================= */
typedef bool (*can_tx_fn)(uint16_t can_id, const uint8_t *data, uint8_t len);

void haltech_set_tx(can_tx_fn fn);

/* Big-endian helper — the Haltech bus is big-endian throughout and the STM32
 * is little-endian, so every 16-bit field needs this. */
static inline uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

#endif /* HALTECH_CAN_H */
