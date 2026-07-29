# Altium Instructions — Dash PCB (`FSAE-DASH`)

> **Permanent memory file.** The dash shares the wheel's platform: **follow
> `wheel-pcb-altium-instructions.md` for every generic step** (project setup §1, libraries §2,
> ERC/DRC gates, output generation §7) — this file documents only what is *different*.
> Companion: `hardware-selections.md`, `system-architecture-and-can.md`, `engineering-rigor.md`.

## 0. Fixed design facts

- **Connector contract:**

**J1 — power, bus & servo signal (DTM04-12P… use positions 1–6, or DTM 6-way if preferred):**

| Pin | Net | Notes |
|---|---|---|
| 1 | `+12V_IN` | vehicle power, 2 A fused on board |
| 2 | `GND` | also the reference for the servo PWM signals |
| 3 | `CAN_H` | Haltech bus, 1 Mbit/s |
| 4 | `CAN_L` | |
| 5 | `SERVO1_PWM` | front ARB signal (5 V logic, buffered) — **reuses the position freed by deleting `+5V_AUX`** |
| 6 | `SERVO2_PWM` | rear ARB signal (was `GND_AUX`) |

**J2 — DAQ (DTM04-12P):**

| Pin | Net | Notes |
|---|---|---|
| 1–6 | `AIN1` … `AIN6` | general DAQ, 0–5 V |
| 7–8 | `AIN7` / `AIN8` | **ARB position feedback** (front/rear) — same 0–5 V front end, no new parts |
| 9 | `+5V_SENS` | 200 mA fused sensor excitation |
| 10–12 | `GND_SENS` | three sensor-return pins |

> **Why the servo signals live on J1, not J2.** Two first-principles reasons, both learned by getting
> the first allocation wrong:
> 1. **Ground-offset integrity.** J2 has eight analog returns sharing its ground pins. Any current
>    flowing in a shared return develops a voltage that appears as offset on *every* DAQ channel.
>    Servo signal returns do not belong in that ground.
> 2. **Reference locality.** A PWM signal must be referenced to the ground its receiver uses. The
>    servo BEC's ground stars back to dash **power** ground, so the servo signals belong on the power
>    connector (J1 pin 2 GND), not the sensor connector.
>
> **Servo power is on neither connector.** Servos are fed by a separate 12V→7.4V BEC branch in the
> harness (~10 A class — a DTM pin is rated 7.5 A and the dash must not carry it). The BEC's ground
> **must** star back to dash `GND`, or the PWM reference floats. See `hardware-selections.md` §9.1 and
> the planned servo power conditioning board in `system-architecture-and-can.md` §3A.4.

- **MCU pin map (STM32G474RET6)** — identical to wheel for CAN/USB/SWD/UART (PB8/PB9, PA11/PA12, PA13/PA14, PA9/PA10). Differences:

| MCU pin | Net | Function |
|---|---|---|
| PA4 / PA5 / PA6 / PA7 | `EVE_CS` / `EVE_SCK` / `EVE_MISO` / `EVE_MOSI` | SPI1 to BT817 (up to 30 MHz after boot) |
| PB3 / PB4* | `EVE_PDN` / `EVE_INT` | display power-down & interrupt (*PB4 has no paddle role here) |
| PA0–PA3, PC0–PC3 | `AIN1_ADC` … `AIN8_ADC` | DAQ channels after front end |
| PB0 / PB1 | `V12_SENSE` / `V5_SENSE` | rail monitors (dividers: 12V→ 47k/10k; 5V→ 10k/10k) |
| PA8 | `LED_DATA_3V3` | optional dash alarm-LED strip (same cell as wheel §3.5, DNP by default) |
| PC8–PC10 | `BTN1`–`BTN3` | optional page/brightness buttons (same conditioning cell) |
| **PB6 / PB7**† | `SERVO1_PWM_3V3` / `SERVO2_PWM_3V3` | **TIM4_CH1/CH2** — ARB servo outputs; two channels on one timer so both servos share a timebase. 50 Hz standard frame (1.0–2.0 ms), firmware may raise to 333 Hz for digital servos |

† **Cross-board pin-reuse note** (same as the PB4 case above): on the *wheel*, PB6 is `LCD_EXTCOMIN`
(also TIM4_CH1). Different boards, no conflict — but firmware sharing one codebase must gate these
behind the board-personality `#define`, not assume a pin means the same thing everywhere.

## 1. Schematic sheets

`dash-power.SchDoc`, `dash-mcu.SchDoc` (copy wheel MCU sheet, edit nets), `dash-can.SchDoc`
(copy wheel CAN sheet **minus paddle circuits**; same DNP split termination policy),
`dash-afe.SchDoc`, `dash-servo.SchDoc`, `dash-display.SchDoc`, `dash-top.SchDoc`.

### 1.1 `dash-power.SchDoc` — 12 V automotive entry
1. `+12V_IN` → **2 A polyfuse** → reverse-polarity **P-FET** (DMP3056L class: source→board, gate→GND via 10k, gate zener 12V) → net `+12V_P`.
2. At the connector side: **SMBJ33A** to GND + 100 nF. (Verify clamp vs. buck abs-max with a scope during load-dump-simulation — rigor assumption; drop to SMBJ26A if measurements demand.)
3. **LMR33630ADDAR** buck → `+5V` @ 2 A, 400 kHz: follow the datasheet reference layout *exactly* (inductor 10 µH ≥3 A sat, 2×22 µF in, 2×47 µF out, RT/FB per 5 V table). Feeds: `+5V_SENS` (through 200 mA polyfuse + SMBJ5.0A at J2), the servo-signal buffers, and on-board 5V loads. **Rev B: the `+5V_AUX` wheel feed, its 1.5 A polyfuse and TVS are deleted** — the wheel is 12V-fed and standalone, which is why this buck drops from 3 A to 2 A.
4. **AP63203WU-7** buck fed from `+12V_P` (per its 3.8–32 V rating, which unloads the 5V buck) → `+3V3` @ 2 A for MCU + display.
5. Ferrite → `+3V3A` for VDDA + dividers' filter caps as on wheel.
6. Test points on every rail; rail monitor dividers to PB0/PB1 with 100 nF.

### 1.2 `dash-afe.SchDoc` — DAQ front end (×8, make it a repeated device sheet)
Per channel (values from `system-architecture-and-can.md` §3):
`AINx (J2) → 10 kΩ series → node N1 → 10 kΩ to GND ∥ 100 nF ∥ BAT54S (clamps to +3V3 and GND) → AINx_ADC`
- All 8 GND legs to a quiet analog pour tied at one point to L2 GND.
- **TLV9004 buffer footprints DNP** between N1 and ADC (jumpered by 0Ω default) — populate only if a future sensor needs low-impedance drive.
- Silkscreen at J2: “0–5 V MAX, 12 V tolerant (fault)”.
- Channels 7 and 8 are the ARB position feedback inputs — electrically identical, so no special handling.

### 1.2b `dash-servo.SchDoc` — ARB servo outputs (×2)
Per channel, in this physical order from MCU to connector:

`PB6/PB7 (3V3 PWM) → 74AHCT2G125 buffer (VCC = +5V) → 100 Ω series → SMAJ5.0A to GND → J1.5/J1.6`

1. **74AHCT2G125** (dual buffer — one package covers both channels): VCC = `+5V` with 100 nF; tie both
   `/OE` pins low. **AHCT** is mandatory, not a preference: its TTL input thresholds (VIH 2.0 V) accept
   3.3 V logic legally, which plain HC does not — the same reasoning as the WS2812 buffer on the wheel.
2. **100 Ω series** on each output, placed at the buffer. This is the fault-current limiter: a 12 V
   short onto a servo line pushes only (12−5)/100 ≈ 70 mA into the clamp instead of destroying the
   buffer. It also damps ringing on a multi-metre run to the suspension.
3. **SMAJ5.0A** from each line to GND, **at the connector** — clamp transients before they travel.
4. Silkscreen at J1: “SERVO SIGNAL ONLY — NO SERVO POWER”.

> Do **not** add a servo power rail to this sheet. See the connector note in §0 and the 74 W
> arithmetic in `hardware-selections.md` §9.1. If a future revision integrates servo power, it belongs
> on the separate conditioning board (`system-architecture-and-can.md` §3A.4), not here.

### 1.3 `dash-display.SchDoc`
20-pin 0.5 mm FPC ZIF to the Riverdi EVE4 module: SPI (CS/SCK/MISO/MOSI + optional QSPI IO2/IO3 — wire them, firmware may upgrade to QSPI), `EVE_PDN`, `EVE_INT`, `+3V3` power pins (check Riverdi datasheet current pinout **against the exact module revision purchased** — transcribe the pin table into the schematic as a note; module revisions have moved pins — rigor lesson L6), bulk 22 µF + 1 µF + 100 nF at the connector.
Backlight is driven on-module; no extra circuit.

## 2. PCB layout deltas

1. Board shape from the dash enclosure/panel DXF; display module mounts *over* the PCB on standoffs with the FPC folding once — place the ZIF within 40 mm of the module's tail exit, contacts facing per the fold direction (mock the fold in paper first — FPC folds are where dashes die).
2. **Power entry corner:** J1 → fuse → FET → TVS → buck, in that physical order, no crossing nets. Buck loops (SW node, input cap loop) minimized per datasheet — the LMR33630 hot loop must be < 20 mm².
3. **Analog zone:** J2 + AFE on the opposite board edge from the bucks; analog pour under AFE; no switching traces within 5 mm; route AINx_ADC on L3 over the analog pour.
4. SPI to display: length < 80 mm, GND-referenced, series 22 Ω at MCU on SCK/MOSI (ringing control at 30 MHz).
5. Same rules/stackup/DRC/outputs as wheel §4–§7 (JLC7628, impedance calc for USB pair; SPI at 30 MHz doesn't need impedance control at these lengths, just referencing).
6. **Servo outputs:** place the 74AHCT2G125 and its series resistors near **J1** (power-entry corner),
   clamps at the connector pins. Keep the two PWM traces well away from the AFE analog zone at the
   opposite board edge — they are 5 V digital edges and the ADC channels are reading millivolt-resolution
   sensors. This separation is why the servo signals were moved to J1 (see §0); the layout and the
   connector allocation are solving the same problem.
7. Silkscreen: both connector pinout tables on back silk; “+12V IN — FUSED 2A” and
   “SERVO SIGNAL ONLY — NO SERVO POWER” at J1; “0–5 V MAX” at J2.

## 3. Firmware/bring-up hooks

- Dash transmits its 8 DAQ channels as emulated IO12 **Box B** frames once assumption A2
  (`system-architecture-and-can.md` §5) is closed; until then they are logged locally/USB only.
- Staged bring-up per `engineering-rigor.md` §4, plus: verify display 3.3 V inrush (A5) with a scope
  before connecting the real module (use a 3.3 Ω resistor dummy first).
- Sunlight test: full-brightness display outdoors at noon behind the actual polycarbonate lens —
  a lens with the wrong AR/haze can undo the 1000-nit panel (rigor lesson L7).
- **ARB servo bring-up (do this on the bench, servos off the car, linkage disconnected):**
  1. Scope both PWM outputs before connecting a servo — confirm 1.0/1.5/2.0 ms pulses and clean 5 V edges.
  2. Verify the **firmware endstops** clamp commands beyond travel *before* the servo is ever bolted
     to a linkage. A servo grinding a hard stop is the most likely way to destroy one.
  3. Confirm the servo BEC ground is starred to dash GND — measure for offset between the two grounds
     under servo load; any significant offset means the reference is wrong and pulse widths will jitter.
  4. Verify CAN-loss behaviour by unplugging the wheel mid-adjustment: the servo must **hold**, not centre.
  5. Verify the divergence alarm by physically stalling the servo output (hand-held, low travel).
