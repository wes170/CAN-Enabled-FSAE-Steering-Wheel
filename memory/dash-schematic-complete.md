# FSAE-DASH Rev B — Complete Schematic Definition

> **This file is self-contained**, with one deliberate exception: the sheets the dash shares
> *identically* with the wheel are not re-typed here, because duplicating them is how the two boards
> drift apart. Where a sheet is shared, this file says so and points at the exact section of
> `wheel-schematic-complete.md` to copy — **copy the Altium sheet, don't redraw it.**
>
> Everything unique to the dash — the DAQ front end, the ARB servo outputs, the display, the
> connectors, the pin map — is fully specified here.
>
> Every number has been read from a datasheet; see `datasheet-verification.md`. Genuinely open items
> are marked **[OPEN]** rather than guessed.

---

## 1. Global nets

Same as the wheel (§1 of `wheel-schematic-complete.md`), plus:

| Net | Meaning |
|---|---|
| `+5V_SENS` | Fused 5 V sensor excitation, out to J2 pin 9 |
| `AIN1`…`AIN8` | Raw 0–5 V analog inputs at J2, before conditioning |
| `AIN1_ADC`…`AIN8_ADC` | Conditioned, divided analog at the MCU pins |
| `SERVO1_PWM_3V3`, `SERVO2_PWM_3V3` | 3.3 V PWM from the MCU |
| `SERVO1_PWM`, `SERVO2_PWM` | Buffered 5 V PWM out to J1 |
| `EVE_*` | Display interface signals |

---

## 2. Shared sheets — copy from the wheel, do not redraw

| Dash sheet | Copy from | Changes to make |
|---|---|---|
| `dash-power.SchDoc` | `wheel-schematic-complete.md` §2.1–2.5 | **⚠ The 3.3 V rail is the one deliberate difference: the dash uses the `AP63203` buck, not the wheel's `AP2112K` LDO (see §7). Otherwise, two changes only:** `F1` becomes a **2 A hold / 4 A trip** polyfuse (1812) instead of 1.1 A; add the `+5V_SENS` branch in §4.3 below. Everything else — Q1/R1/D5/D1, the LMR36015 and all its passives, FB1, the rail-monitor dividers — is identical |
| `dash-mcu.SchDoc` | `wheel-schematic-complete.md` §3 | Identical, including **the HSE crystal on PF0/PF1 (§3.2 — mandatory for 1 Mbit CAN, not optional; order `ABM8-16.000MHZ-8-D4Y-T` by the full option string, because the ABM8's *standard* part is CL 18 pF / −10…+60 °C and would leave only 1.2× startup margin — see the ordering box in wheel §3.2)** and **fitting nothing on BOOT0** (`PB8-BOOT0` is `FDCAN1_RX` on this board too). Rail monitors land on PB0/PB1 here instead of PA1/PA2 — see §8 |
| `dash-can.SchDoc` | `wheel-schematic-complete.md` §4.1–4.2 | **Delete the paddle circuits (§4.3) entirely.** The dash has no paddles. Keep U3, its decoupling, D2 and the DNP termination |

The LMR36015 sizing holds for the dash: its load is ~1.0 A (see §7) against a 1.5 A rating.

---

## 3. Sheet `dash-afe.SchDoc` — DAQ front end, ×8

Draw once as a device sheet, instantiate 8 times for `n = 1…8`.

```
AINn (J2) ──[ Rs 10 kΩ ]──┬── AINn_ADC  (to MCU)
                          ├── Rg 10 kΩ ── GND
                          ├── Cf 100 nF ── GND
                          └── BAV199: upper anode→node, cathode→+3V3
                                      lower cathode→node, anode→GND
```

| Ref | Part / value | Notes |
|---|---|---|
| `R9`–`R16` | 10 kΩ 1 %, 0402 | series limiter, one per channel |
| `R17`–`R24` | 10 kΩ 1 %, 0402 | lower divider leg |
| `C24`–`C31` | 100 nF, 0402 | anti-alias / filter |
| `D6`–`D13` | **BAV199** dual low-leakage silicon, SOT-23 | **not BAT54S** |
| `U6` | TLV9004 quad op-amp, TSSOP-14 | **DNP.** 0 Ω jumpers bypass it by default |

**BAV199 pin-out — the diodes are in SERIES, and that is what makes one 3-pin package do both jobs.**
Nexperia BAV199 data sheet, 1 April 2023, §1 and Table 2 (`hardware/lib/datasheet-BAV199-nexperia.pdf`):

| Pin | Datasheet symbol | What it is | **Connect to** |
|---|---|---|---|
| **1** | `A1` | anode of diode 1 | **`GND`** |
| **2** | `K2` | cathode of diode 2 | **`+3V3`** |
| **3** | `K1, A2` | cathode of D1 **and** anode of D2 — the internal series junction | **the sense net** |

The sense net lands on **one pin, not two**. "Upper diode anode to the net, lower diode cathode to the
net" describes the *electrical* intent correctly and reads as though the net connects twice; it does
not. Pin 3 is both of those terminals at once, because the two diodes already meet inside the package.

Check it by walking each diode:
- **Pin 3 below ground** → D1 conducts pin 1 → pin 3, i.e. `GND` into the net. **Lower clamp.**
- **Pin 3 above `+3V3`** → D2 conducts pin 3 → pin 2, i.e. the net into `+3V3`. **Upper clamp.**

⚠ **A SOT-23 dual diode is the classic footgun**: BAV70 (common cathode), BAW56 (common anode) and
BAV99/BAV199 (series) share a package, a silkscreen and an almost identical schematic symbol, and only
the series part can clamp both rails from one net. Substituting "an equivalent SOT-23 dual" silently
breaks this circuit.


**Why BAV199 and not a Schottky:** the divider presents a 5 kΩ Thévenin source at the ADC node, and
clamp leakage flowing into that impedance *is* signal. A BAT54S leaks 2 µA at 25 °C and ~100 µA at
100 °C → 10 mV of offset cold and **500 mV hot**, a 20 % error on a 0–2.5 V channel in a dash we
specified for direct sun. BAV199 leaks ~3 pA. Its higher forward drop costs nothing on a fault path.

**Scaling:** divide-by-2, so 0–5 V in → 0–2.5 V at the ADC, leaving 0.8 V of over-range headroom
below the 3.3 V reference. Filter corner ≈ 1/(2π · 5 kΩ · 100 nF) ≈ **320 Hz**, right for
temperatures and pressures; retune `Cf` per channel if a fast signal is ever needed.

**Fault behaviour — the clamp is PRIMARY protection, not a backup.** The STM32G474 datasheet
(Table 15) gives `IINJ(PIN)` as **−5 / 0 mA**: the −5 mA applies to *negative* injection only, and
note 3 states positive injection "is not possible on these I/Os". There is therefore **no sanctioned
path for the MCU to absorb a positive overvoltage** — Table 14 simply caps TT-pin input voltage at
**4.0 V absolute maximum**. So `D6`–`D13` are load-bearing: **never depopulate them.**
A 12 V fault gives `(12 − 3.3 − Vf)/10 kΩ ≈ 0.8 mA`; at that current a small-signal silicon diode
drops ~0.55–0.6 V, holding the pin near **3.9 V** against the 4.0 V limit. That passes with only
~0.1 V of margin — **flagged for the Step 2 design pass** (`datasheet-verification.md` defect 1.7),
along with the fact that the `AIN` lines currently have **no TVS at the connector**.

**Grounding:** all eight `Rg` and `Cf` returns go to a dedicated analog pour, tied to the main ground
at **one point**. Do not let servo or digital return current share it.

---

## 4. Sheet `dash-servo.SchDoc` — ARB outputs, ×2

### 4.1 Signal chain per channel

```
PB6 / PB7 (3.3 V PWM) ── U7 74AHCT2G125 (VCC = +5V) ── R(100 Ω) ──┬── J1.5 / J1.6
                                                                  └── SMAJ5.0A ── GND
```

| Ref | Part / value | Connections |
|---|---|---|
| `U7` | **74AHCT2G125**, VSSOP-8, dual buffer | VCC → `+5V`, GND → `GND`, both `/OE` → `GND`. A1 ← `SERVO1_PWM_3V3` (PB6), A2 ← `SERVO2_PWM_3V3` (PB7); Y1 → `NET_SV1`, Y2 → `NET_SV2` |
| `C32` | 100 nF, 0402 | `+5V` → `GND` at U7 |
| `R25`, `R26` | 100 Ω 1 %, 0805 | `NET_SV1` → `SERVO1_PWM`, `NET_SV2` → `SERVO2_PWM`; place at the buffer |
| `D14`, `D15` | **SMAJ5.0A**, SMA | each `SERVOn_PWM` → `GND`, **at the connector** |

**AHCT is required, not preferred** — its TTL input threshold (V_IH 2.0 V) legally accepts 3.3 V
logic, and servos expect a ~5 V pulse. **The 100 Ω is the fault limiter:** a 12 V short onto a servo
line pushes only (12 − 5)/100 ≈ **70 mA** into the clamp instead of destroying the buffer, and it
damps ringing on a multi-metre run to the suspension.

### 4.2 What must NOT be on this sheet

**No servo power rail.** Two high-torque ARB servos are ~10 A stall / ~74 W, against a 7.5 A DTM pin
rating — that current must not cross this PCB or its connectors. Servo power is a separate
12 V→7.4 V BEC branch in the harness with its own fuse, **and its ground must star back to dash
`GND`** or the PWM reference floats and pulse widths jitter. If servo power is ever integrated, it
belongs on the planned conditioning board, not here.

---

## 5. Sheet `dash-display.SchDoc` — Riverdi RVT50HQBNWN00

`J4` = 20-pin 0.5 mm ZIF. Matched cable `FFC0520150`. Pinout verified against DS Rev 1.7:

| Pin | Signal | Connect to |
|---|---|---|
| 1 | VDD | `+3V3` (module logic: 98 mA typ, 384 mA max) |
| 2 | GND | `GND` |
| 3 | SPI_SCLK | `EVE_SCK` (PA5) |
| 4 | MISO / IO1 | `EVE_MISO` (PA6) |
| 5 | MOSI / IO0 | `EVE_MOSI` (PA7) |
| 6 | CS | `EVE_CS` (PA4) |
| 7 | INT | `EVE_INT` (PB4) — active low, **internally pulled up 47 kΩ**, add nothing |
| 8 | RST/PD | `EVE_PDN` (PB3) — active low, **internally pulled up 47 kΩ** |
| 9 | GPIO.0 | spare — bring to a test point |
| 10 | DISP_AUDIO | leave open (no speaker fitted) |
| 11 | GPIO.1 / IO2 | `EVE_IO2` — route it; firmware may upgrade to QSPI |
| 12 | GPIO.2 / IO3 | `EVE_IO3` — same |
| 13–16 | NC | leave open |
| **17, 18** | **BLVDD** | **`+5V` — both pins.** Backlight is a *separate* supply |
| **19, 20** | **BLGND** | `GND` — both pins |

| Ref | Value | Connection |
|---|---|---|
| `C33` | 22 µF, 16 V, X7R, 0805 | `+3V3` → `GND` at J4 |
| `C34` | 1 µF, 0603 | `+3V3` → `GND` at J4 |
| `C35` | 100 nF, 0402 | `+3V3` → `GND` at J4 |
| `C35b` | **22 µF, 16 V, 0805** | **`+5V` → `GND` at pins 17/18** — the backlight is the switching load, it needs its own bulk |
| `R27`, `R28` | 22 Ω, 0402 | in series on `EVE_SCK` and `EVE_MOSI`, **at the MCU end**. These are **series termination**: the MCU's output impedance is well below the trace's characteristic impedance, so a fast edge reflects off the far end and rings. Adding ~22 Ω brings the source closer to the trace impedance and damps that reflection. The "30 MHz" refers to the BT817's maximum SPI clock — the faster the edges, the more the trace behaves like a transmission line and the more this matters. Place them at the driver, not the receiver; at the far end they do nothing |

**The backlight is not on the 3.3 V rail.** `BLVDD` is its own input, spec 3.1 / **5.0 typ** / 5.5 V,
drawing **353 mA at 5 V** at full brightness. Feed it from `+5V`: the driver is constant-current, so
at 3.3 V it would draw **657 mA** for the same light. Logic levels are V_IH 2.0 V / V_IL 0.8 V, so
3.3 V GPIO drives it directly.

---

## 6. Sheets `dash-buttons` and connectors

### 6.1 Local buttons (optional, 3 off)
Same conditioning cell as the wheel (`wheel-schematic-complete.md` §5.1):
`SW1`–`SW3` = C&K KSC4 → `BTN1`–`BTN3`; `R29`–`R31` 1 kΩ series; `R32`–`R34` 10 kΩ pull-ups;
`C36`–`C38` 100 nF.

### 6.2 Alarm LED strip (optional)
`U8` = 74AHCT1G125 driving an off-board strip from `LED_DATA_3V3` (PA8). **DNP by default**; wire the
footprint and a JST-GH 3-pin (`+5V`, data, `GND`) so it can be fitted later.

### 6.3 J1 — power, bus and servo signal (DTM04-12P, positions 1–6)

| Pin | Net |
|---|---|
| 1 | `+12V_IN` |
| 2 | `GND` — **also the reference for the servo PWM signals** |
| 3 | `CAN_H` |
| 4 | `CAN_L` |
| 5 | `SERVO1_PWM` |
| 6 | `SERVO2_PWM` |

Silkscreen at J1: `+12V IN — FUSED 2A` and `SERVO SIGNAL ONLY — NO SERVO POWER`.

### 6.4 J2 — DAQ (DTM04-12P)

| Pin | Net |
|---|---|
| 1–6 | `AIN1` … `AIN6` |
| 7, 8 | `AIN7`, `AIN8` — ARB position feedback (front, rear) |
| 9 | `+5V_SENS` |
| 10–12 | `GND_SENS` (three return pins) |

`+5V_SENS` branch: `+5V` → `F2` 200 mA polyfuse (0805) → `+5V_SENS`, with `D2b` **SMBJ5.0A** from
`+5V_SENS` to `GND` at the connector. Silkscreen: `0–5 V MAX`.

**Servo signals live on J1, not J2, on purpose.** J2's grounds carry eight analog returns; any current
in a shared return becomes offset on *every* DAQ channel. A PWM signal must also reference the ground
its receiver uses, and the servo BEC stars to power ground. Do not "tidy" the servo pins onto J2.

---

## 7. Power budget (verified numbers)

| Load | Rail | Current |
|---|---|---|
| Display backlight (`BLVDD`) @ 100 % | 5 V | 0.353 A |
| Display module logic (`VDD`) | 3.3 V | 0.098 A typ (0.384 A abs max, audio only) |
| MCU + CAN + analog | 3.3 V | 0.10 A |
| Sensor excitation | 5 V | 0.20 A |
| Servo buffers | 5 V | <0.01 A |

- **3.3 V total ≈ 0.5 A**, supplied by the **AP63203 buck** (`U2`), *not* an LDO. At 0.5 A an LDO from
  5 V would burn 0.5 × 1.7 = **0.85 W in a SOT-25**, which is why the dash keeps a switcher here while
  the wheel (≈100 mA, 0.17 W) uses the AP2112K LDO. **The two boards deliberately differ on this one
  part** — do not "harmonise" them.
- **5 V total ≈ 0.56 A direct + 0.37 A reflected from the 3.3 V *buck* ≈ 0.93 A** → LMR36015 (1.5 A) ✅
  (0.353 backlight + 0.20 sensor excitation + 0.01 servo buffers = 0.563 A direct. The reflected term
  is a **buck**, not an LDO, so it is power-conserving, not current-conserving:
  0.5 A × 3.3 V / 5 V / 0.9 = 0.37 A. Through an LDO it would have been the full 0.5 A.)
- **12 V input ≈ 4.65 W / 0.85 / 12 V ≈ 0.46 A** → 2 A polyfuse ✅ (4.3× margin)

> **The 0.85 W LDO-dissipation worry does not apply here** and earlier revisions of this section left
> text implying it did. The dash's 3.3 V rail is the **AP63203 buck**; an LDO at 0.5 A would have
> burned 0.5 × 1.7 = 0.85 W in a small package, which is *why* this board uses a switcher while the
> wheel (≈100 mA, 0.17 W) uses the AP2112K LDO. **The two boards deliberately differ on this one
> part — do not "harmonise" them.** Closed, not open.

---

### 8.0 Complete pin assignment — **all 64 pins, in package order**

Pin numbers from **DS12288 Rev 6, Figure 7 "LQFP64 pinout"** (top view). Same package and same
numbering as the wheel — but **the nets are different**, so do not copy the wheel's pin table across.
`dash-mcu.SchDoc` copies the wheel's *sheet structure*; the signal assignment below is what changes.

| Pin | Pin name | Net | Notes |
|---|---|---|---|
| 1 | `VBAT` | `+3V3` | No coin cell. **Must not float.** `C16` 100 nF at the pin |
| 2 | `PC13` | **spare** | Free on the dash (it is `BTN1` on the wheel) |
| 3 | `PC14-OSC32_IN` | *no connect* | No LSE fitted |
| 4 | `PC15-OSC32_OUT` | *no connect* |  |
| 5 | `PF0-OSC_IN` | `NET_OSC_IN` | `Y1`.1 + `C_X1` — same crystal circuit as the wheel |
| 6 | `PF1-OSC_OUT` | `NET_OSC_OUT` | → `R_X1` → `NET_XOUT` → `Y1`.3 + `C_X2` |
| 7 | `PG10-NRST` | `NRST` | `C23` 100 nF to `GND` |
| 8 | `PC0` | `AIN5_ADC` | ADC12_IN6 |
| 9 | `PC1` | `AIN6_ADC` | ADC12_IN7 |
| 10 | `PC2` | `AIN7_ADC` | ADC12_IN8 — **ARB front position feedback** |
| 11 | `PC3` | `AIN8_ADC` | ADC12_IN9 — **ARB rear position feedback** |
| 12 | `PA0` | `AIN1_ADC` | ADC12_IN1 |
| 13 | `PA1` | `AIN2_ADC` | ADC12_IN2 |
| 14 | `PA2` | `AIN3_ADC` | ADC1_IN3 |
| 15 | `VSS` | `GND` |  |
| 16 | `VDD` | `+3V3` | `C17` 100 nF at the pin |
| 17 | `PA3` | `AIN4_ADC` | ADC1_IN4 |
| 18 | `PA4` | `EVE_CS` | SPI1_NSS — driven as GPIO |
| 19 | `PA5` | `EVE_SCK` | SPI1_SCK, **≤ 30 MHz** |
| 20 | `PA6` | `EVE_MISO` | SPI1_MISO |
| 21 | `PA7` | `EVE_MOSI` | SPI1_MOSI |
| 22 | `PC4` | **spare** |  |
| 23 | `PC5` | **spare** |  |
| 24 | `PB0` | `V12_SENSE` | **ADC1_IN15** — 47 k/10 k divider |
| 25 | `PB1` | `V5_SENSE` | **ADC1_IN12** — 10 k/10 k divider |
| 26 | `PB2` | **spare** |  |
| 27 | `VSSA` | `GND` | Analog ground, one point back to `GND` |
| 28 | `VREF+` | `+3V3A` |  |
| 29 | `VDDA` | `+3V3A` | Via `FB1`; `C10` 1 µF + `C11` 100 nF |
| 30 | `PB10` | **spare** |  |
| 31 | `VSS` | `GND` |  |
| 32 | `VDD` | `+3V3` | `C18` 100 nF at the pin |
| 33 | `PB11` | **spare** |  |
| 34 | `PB12` | **spare** |  |
| 35 | `PB13` | **spare** |  |
| 36 | `PB14` | **spare** |  |
| 37 | `PB15` | **spare** |  |
| 38 | `PC6` | **spare** |  |
| 39 | `PC7` | **spare** |  |
| 40 | `PC8` | `BTN1` | Local button, conditioning cell as the wheel |
| 41 | `PC9` | `BTN2` |  |
| 42 | `PA8` | `LED_DATA_3V3` | TIM1_CH1 + DMA → alarm strip buffer |
| 43 | `PA9` | `DBG_TX` | USART1_TX → `J5`. ⚠ `UCPD1_DBCC1` — see §8.1 |
| 44 | `PA10` | `DBG_RX` | USART1_RX → `J5`. ⚠ `UCPD1_DBCC2` — see §8.1 |
| 45 | `PA11` | `USB_DM` | To `U5` USBLC6 |
| 46 | `PA12` | `USB_DP` | To `U5` USBLC6 |
| 47 | `VSS` | `GND` |  |
| 48 | `VDD` | `+3V3` | `C19` 100 nF at the pin |
| 49 | `PA13` | `SWDIO` | TC2030 pad |
| 50 | `PA14` | `SWCLK` | TC2030 pad |
| 51 | `PA15` | **spare** |  |
| 52 | `PC10` | `BTN3` |  |
| 53 | `PC11` | **spare** |  |
| 54 | `PC12` | **spare** |  |
| 55 | `PD2` | **spare** |  |
| 56 | `PB3` | `EVE_PDN` | Active low; module pulls up 47 kΩ internally — **drive it** |
| 57 | `PB4` | `EVE_INT` | Active low, 47 kΩ internal pull-up. ⚠ `UCPD1_CC2` — firmware **must** set `PWR_CR3.UCPD1_DBDIS` (§8.1) |
| 58 | `PB5` | **spare** |  |
| 59 | `PB6` | `SERVO1_PWM_3V3` | TIM4_CH1 → `U7` buffer. ⚠ `UCPD1_CC1` — benign here (push-pull output) but see §8.1 |
| 60 | `PB7` | `SERVO2_PWM_3V3` | TIM4_CH2 → `U7` buffer |
| 61 | `PB8-BOOT0` | `CAN_RX` | FDCAN1_RX. ⚠ **Fit nothing else on this pin** |
| 62 | `PB9` | `CAN_TX` | FDCAN1_TX |
| 63 | `VSS` | `GND` |  |
| 64 | `VDD` | `+3V3` | `C20` 100 nF at the pin |

#### Power pins
The LQFP-64 has **four `VDD` pins (16, 32, 48, 64)** and **four `VSS` pins (15, 31, 47, 63)**, plus
`VBAT` (1), `VDDA` (29), `VREF+` (28) and `VSSA` (27). Decoupling: `C16` at `VBAT`, `C17`–`C20` one
at each `VDD`, `C10`/`C11` at `VDDA`, `C22` 4.7 µF bulk, `C23` 100 nF at `NRST`.

#### Spare pins on this board
`PC13`, `PC4`, `PC5`, `PB2`, `PB10`–`PB15`, `PC6`, `PC7`, `PA15`, `PC11`, `PC12`, `PD2`, `PB5` — the
dash uses far fewer I/O than the wheel. Bring a few to test points; leave the rest unconnected and
configure them as analog inputs in firmware (lowest leakage for an unused pin).

⚠ **`PB4` is spare-adjacent but is `EVE_INT`, and `PB6` is `UCPD1_CC1`.** If you ever repurpose a
spare pin, re-read §8.1 first.

---

## 8. MCU signal map (summary — the authoritative per-pin table is §8.0 above) (verified against STM32G474 datasheet Table 13)

| Pin | Net | Justification |
|---|---|---|
| PA4 / PA5 / PA6 / PA7 | `EVE_CS` / `EVE_SCK` / `EVE_MISO` / `EVE_MOSI` | SPI1_NSS / SCK / MISO / MOSI — a complete SPI1 set |
| PB3 / PB4 | `EVE_PDN` / `EVE_INT` | GPIO. ⚠ **PB4 is also `UCPD1_CC2`** — firmware must set `PWR_CR3.UCPD1_DBDIS` or `EVE_INT` reads permanently asserted with a debug adapter attached (defect 8.2, §8.1) |
| PA0–PA3, PC0–PC3 | `AIN1_ADC` … `AIN8_ADC` | **All eight reach ADC1**, so one scan sequence covers them — nothing stranded on ADC3/4/5. Channels, verified in DS12288 Table 12: **PA0–PA3 = IN1, IN2, IN3, IN4; PC0–PC3 = IN6, IN7, IN8, IN9.** Do **not** derive these from pin numbers and do not leave them to CubeMX — that assumption was defect 8.3 on the wheel |
| PB0 / PB1 | `V12_SENSE` / `V5_SENSE` | ADC1/ADC2 capable. **Why not PA1/PA2 as on the wheel:** on the dash, PA0–PA3 are all consumed by DAQ channels `AIN1`–`AIN4`, so the rail monitors had to move. Pure pin pressure, no electrical reason — which is exactly why the two boards' pin maps must never be assumed identical |
| **PB6 / PB7** | `SERVO1_PWM_3V3` / `SERVO2_PWM_3V3` | **TIM4_CH1 / TIM4_CH2** — one timer, so both servos share a timebase |
| PA8 | `LED_DATA_3V3` | TIM1_CH1 (no encoder competes for TIM1 on this board) |
| PC8–PC10 | `BTN1`–`BTN3` | GPIO input |
| PB8 / PB9 | `CAN_RX` / `CAN_TX` | FDCAN1 — **fit nothing on BOOT0** |
| PA11 / PA12 | `USB_DM` / `USB_DP` | USB FS |
| PA13 / PA14 | `SWDIO` / `SWCLK` | debug |
| PA9 / PA10 | `DBG_TX` / `DBG_RX` | USART1. ⚠ **Also `UCPD1_DBCC1` / `UCPD1_DBCC2`** — see §8.1 |
| **PF0 / PF1** | `OSC_IN` / `OSC_OUT` | **HSE crystal, LQFP-64 pins 5/6 — required for CAN bit timing** |

### ⚠ 8.1 The UCPD dead-battery trap (defect 8.2)

Identical mechanism to the wheel — see `wheel-schematic-complete.md` §9.1 for the datasheet quote and
the full derivation. On **this** board the affected pins are:

- **PB4 = `EVE_INT`**, armed by a high on PA10 (`DBG_RX` — a debug adapter's TX idles high). The
  Riverdi module pulls INT up with 47 kΩ internally; 5.1 kΩ against that gives **0.32 V**, so the
  active-low display interrupt reads permanently asserted.
- **PB6 = `SERVO1_PWM_3V3`**, armed by a high on PA9 (`DBG_TX`). This one is benign — a push-pull
  output overpowers 5.1 kΩ, costing 0.65 mA — but it is the same coupling and worth knowing about.

Same fix, same place, both boards: `PWR->CR3 |= PWR_CR3_UCPD1_DBDIS;` before any GPIO setup.
**Bring-up gate:** with a debug adapter attached, confirm `EVE_INT` reads high while the display idles.

**Cross-board note:** PB6/PB7 are the servo pair here and `ENC3_A/B` on the wheel; PB3/PB4 are display
control here; on the wheel PB3 is `ENC4_B` and **PB4 is spare** (the paddle taps are PB0/PB1). Different boards, no conflict — but the shared
firmware must gate these behind the board-personality `#define` and never assume a pin means the same
thing on both.

---

## 9. Remaining open items

| Item | Impact | Closes by |
|---|---|---|
| ~~3.3 V LDO dissipation~~ | **Closed** — the dash uses the AP63203 *buck* on this rail, not an LDO, so the 0.85 W concern does not arise | — |
| LMR36015 variant fSW | 400 kHz part needs L = 15 µH, C_OUT = 3 × 22 µF | Ordering-table lookup for the part you buy |
| SMAJ5.0A / SMBJ5.0A parameters | Low risk — standoff clearly exceeds the 5 V rail | Quick check at G6 |

Nothing here blocks starting the schematic.
