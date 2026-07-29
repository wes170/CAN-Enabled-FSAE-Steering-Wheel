# Altium Instructions — Dash PCB (`FSAE-DASH`)

> **Permanent memory file.** The dash shares the wheel's platform: **follow
> `wheel-pcb-altium-instructions.md` for every generic step** (project setup §1, libraries §2,
> ERC/DRC gates, output generation §7) — this file documents only what is *different*.
> Companion: `hardware-selections.md`, `system-architecture-and-can.md`, `engineering-rigor.md`.

## 0. Fixed design facts

- **Connector contract:**

**J1 — power & bus (DTM04-12P... use positions 1–6, or DTM 6-way if preferred):**

| Pin | Net | Notes |
|---|---|---|
| 1 | `+12V_IN` | vehicle power, 2 A fused on board |
| 2 | `GND` | |
| 3 | `CAN_H` | Haltech bus, 1 Mbit/s |
| 4 | `CAN_L` | |
| 5 | `+5V_AUX` | **output** → wheel pin 1 (1.5 A polyfuse) |
| 6 | `GND_AUX` | wheel return (star at dash) |

**J2 — DAQ (DTM04-12P):**

| Pin | Net |
|---|---|
| 1–8 | `AIN1` … `AIN8` (0–5 V) |
| 9 | `+5V_SENS` (200 mA fused sensor excitation) |
| 10–12 | `GND_SENS` |

- **MCU pin map (STM32G474RET6)** — identical to wheel for CAN/USB/SWD/UART (PB8/PB9, PA11/PA12, PA13/PA14, PA9/PA10). Differences:

| MCU pin | Net | Function |
|---|---|---|
| PA4 / PA5 / PA6 / PA7 | `EVE_CS` / `EVE_SCK` / `EVE_MISO` / `EVE_MOSI` | SPI1 to BT817 (up to 30 MHz after boot) |
| PB3 / PB4* | `EVE_PDN` / `EVE_INT` | display power-down & interrupt (*PB4 has no paddle role here) |
| PA0–PA3, PC0–PC3 | `AIN1_ADC` … `AIN8_ADC` | DAQ channels after front end |
| PB0 / PB1 | `V12_SENSE` / `V5_SENSE` | rail monitors (dividers: 12V→ 47k/10k; 5V→ 10k/10k) |
| PA8 | `LED_DATA_3V3` | optional dash alarm-LED strip (same cell as wheel §3.5, DNP by default) |
| PC8–PC10 | `BTN1`–`BTN3` | optional page/brightness buttons (same conditioning cell) |

## 1. Schematic sheets

`dash-power.SchDoc`, `dash-mcu.SchDoc` (copy wheel MCU sheet, edit nets), `dash-can.SchDoc`
(copy wheel CAN sheet **minus paddle circuits**; same DNP split termination policy),
`dash-afe.SchDoc`, `dash-display.SchDoc`, `dash-top.SchDoc`.

### 1.1 `dash-power.SchDoc` — 12 V automotive entry (new content)
1. `+12V_IN` → **2 A polyfuse** → reverse-polarity **P-FET** (DMP3056L class: source→board, gate→GND via 10k, gate zener 12V) → net `+12V_P`.
2. At the connector side: **SMBJ33A** to GND + 100 nF. (Verify clamp vs. buck abs-max with a scope during load-dump-simulation — rigor assumption; drop to SMBJ26A if measurements demand.)
3. **LMR33630ADDAR** buck → `+5V` @ 3 A, 400 kHz: follow the datasheet reference layout *exactly* (inductor 10 µH ≥4 A sat, 2×22 µF in, 2×47 µF out, RT/FB per 5 V table). Feeds: `+5V_AUX` (through 1.5 A polyfuse + SMBJ5.0A at J1), `+5V_SENS` (through 200 mA polyfuse + SMBJ5.0A at J2), on-board 5V loads.
4. **AP63203WU-7** buck `+5V` (or `+12V_P` — datasheet allows; from 5V keeps EMI chain single-stage... feed from `+12V_P` per its 3.8–32 V rating to unload the 5V buck: chosen) → `+3V3` @ 2 A for MCU + display.
5. Ferrite → `+3V3A` for VDDA + dividers' filter caps as on wheel.
6. Test points on every rail; rail monitor dividers to PB0/PB1 with 100 nF.

### 1.2 `dash-afe.SchDoc` — DAQ front end (×8, make it a repeated device sheet)
Per channel (values from `system-architecture-and-can.md` §3):
`AINx (J2) → 10 kΩ series → node N1 → 10 kΩ to GND ∥ 100 nF ∥ BAT54S (clamps to +3V3 and GND) → AINx_ADC`
- All 8 GND legs to a quiet analog pour tied at one point to L2 GND.
- **TLV9004 buffer footprints DNP** between N1 and ADC (jumpered by 0Ω default) — populate only if a future sensor needs low-impedance drive.
- Silkscreen at J2: “0–5 V MAX, 12 V tolerant (fault)”.

### 1.3 `dash-display.SchDoc`
20-pin 0.5 mm FPC ZIF to the Riverdi EVE4 module: SPI (CS/SCK/MISO/MOSI + optional QSPI IO2/IO3 — wire them, firmware may upgrade to QSPI), `EVE_PDN`, `EVE_INT`, `+3V3` power pins (check Riverdi datasheet current pinout **against the exact module revision purchased** — transcribe the pin table into the schematic as a note; module revisions have moved pins — rigor lesson L6), bulk 22 µF + 1 µF + 100 nF at the connector.
Backlight is driven on-module; no extra circuit.

## 2. PCB layout deltas

1. Board shape from the dash enclosure/panel DXF; display module mounts *over* the PCB on standoffs with the FPC folding once — place the ZIF within 40 mm of the module's tail exit, contacts facing per the fold direction (mock the fold in paper first — FPC folds are where dashes die).
2. **Power entry corner:** J1 → fuse → FET → TVS → buck, in that physical order, no crossing nets. Buck loops (SW node, input cap loop) minimized per datasheet — the LMR33630 hot loop must be < 20 mm².
3. **Analog zone:** J2 + AFE on the opposite board edge from the bucks; analog pour under AFE; no switching traces within 5 mm; route AINx_ADC on L3 over the analog pour.
4. SPI to display: length < 80 mm, GND-referenced, series 22 Ω at MCU on SCK/MOSI (ringing control at 30 MHz).
5. Same rules/stackup/DRC/outputs as wheel §4–§7 (JLC7628, impedance calc for USB pair; SPI at 30 MHz doesn't need impedance control at these lengths, just referencing).
6. Silkscreen: both connector pinout tables on back silk; “+12V IN — FUSED 2A” warning at J1.

## 3. Firmware/bring-up hooks

- Dash transmits its 8 DAQ channels as emulated IO12 **Box B** frames once assumption A2
  (`system-architecture-and-can.md` §5) is closed; until then they are logged locally/USB only.
- Staged bring-up per `engineering-rigor.md` §4, plus: verify display 3.3 V inrush (A5) with a scope
  before connecting the real module (use a 3.3 Ω resistor dummy first).
- Sunlight test: full-brightness display outdoors at noon behind the actual polycarbonate lens —
  a lens with the wrong AR/haze can undo the 1000-nit panel (rigor lesson L7).
