# The Dash Board, Explained in Plain English

This is a companion to `memory/dash-schematic-complete.md` (the real spec — every number in this
guide is copied from it, not re-derived). If the two ever disagree, the schematic file is right and
this one has a typo. The goal here is just to walk through *what* the dash PCB does, *why* it's built
the way it is, and *what you're about to draw in Altium* — in words, before you're staring at a
component on a grid.

A few abbreviations that will come up constantly: **PCB** = printed circuit board (the physical
board), **MCU** = microcontroller (the chip that runs the firmware — on this board it's an STM32G474),
**CAN bus** = Controller Area Network, the two-wire data bus that lets every module on the car (wheel,
dash, ECU, data logger) talk on one shared line, **PWM** = pulse-width modulation, a digital signal
that encodes a value (like "servo angle") as the width of repeating pulses, **ADC** = analog-to-digital
converter, the circuit inside the MCU that turns a voltage into a number the firmware can read.

---

## 1. The big picture: what does this board actually do?

The dash board has five jobs:

1. **Read eight analog sensor channels** (temperatures, pressures, whatever the team wires up) and
   hand clean numbers to the MCU — this is the "DAQ front end" (DAQ = data acquisition).
2. **Drive two servo motors that adjust the car's anti-roll bars (ARBs)** in response to commands the
   driver dials in from the wheel — over CAN, with no new wiring back to the wheel needed.
3. **Drive a 5-inch colour display** (a Riverdi module) so the driver can see live data.
4. **Talk CAN** to the rest of the car — same bus, same protocol chip family as the wheel.
5. **Optionally** read a few local buttons and drive an off-board LED alarm strip.

Three of the sheets that make this happen — power, MCU, and CAN — are not drawn fresh for the dash.
They are copied, almost unchanged, from the wheel project. Why that matters is worth explaining before
anything else, because it's the first thing you'll do in Altium and it's easy to think "I'll just redo
it my way."

---

## 2. Sheets copied from the wheel — and why you must not redraw them

Three Altium sheets on the dash — `dash-power.SchDoc`, `dash-mcu.SchDoc`, `dash-can.SchDoc` — are
**copies of the wheel's equivalent sheets**, not new designs. The instruction in the source document
is blunt about this: **"copy the Altium sheet, don't redraw it."**

The reasoning: if two boards need the same circuit (same power regulator, same MCU wiring, same CAN
transceiver), and each board's designer redraws it from scratch, small differences creep in — a
resistor value here, a decoupling cap there — that nobody intended and nobody notices until a board
comes back from fab acting strangely. A copied sheet cannot drift from its source, because it *is* the
source. A redrawn sheet can, silently, one small choice at a time. So: literally copy the Altium sheet
object, then apply only the specific edits below.

**`dash-power.SchDoc`** — copy from the wheel's power sections and make exactly two changes:
- `F1` (the main input fuse, a resettable "polyfuse" — a small component that trips like a fuse but
  resets itself once the fault clears and it cools down) becomes a **2 A hold / 4 A trip** part in an
  **1812** package (a physical size code — larger than the wheel's fuse footprint), replacing the
  wheel's 1.1 A part.
- Add the `+5V_SENS` branch (covered in §5 below — this is the fused 5 V feed the DAQ connector
  supplies to sensors).

Everything else on that sheet — the reverse-polarity protection FET `Q1`/`R1`/`D5`/`D1`, the LMR36015
switching regulator (a "buck converter" — it steps 12 V down to 5 V efficiently) and its passive
components, the `AP2112K` linear regulator (5 V → 3.3 V), `FB1`, and the rail-monitor voltage dividers
— is identical to the wheel. The LMR36015's sizing still holds for the dash too: its total load here
is about 1.0 A against a 1.5 A rating (see the power budget in §6).

**`dash-mcu.SchDoc`** — copy from the wheel's MCU sheet, identical, with two things called out
explicitly because they're easy to get wrong on any STM32G4 board:
- **The HSE crystal on pins PF0/PF1 is mandatory, not optional.** ("HSE" = high-speed external — an
  actual crystal oscillator, as opposed to the chip's internal, less precise clock.) CAN running at
  1 Mbit/s needs a timing reference accurate enough that the internal oscillator isn't good enough —
  skip the crystal and the CAN bus will misbehave.
  - **Fit nothing on BOOT0 — no pulldown, no strap, no test point.** On this chip PB8-BOOT0 is also
    `FDCAN1_RX`, so a pulldown there fights the CAN transceiver's RXD output: actively harmful, not
    merely redundant. And if BOOT0 is ever taken from the pin, an idle CAN bus sits recessive, which
    is logic HIGH — so the MCU samples BOOT0 = 1 and jumps into the system bootloader at every
    power-on with a live bus, while booting perfectly on the bench with the bus unplugged. BOOT0 must
    come from the `nBOOT0` option bit with `nBOOT_SEL = 1`, set at first flash, verified on every
    board, and re-checked after any mass erase, because a full chip erase can restore the factory
    option-bit state. DFU entry uses USB DFU or an SWD-triggered jump — never a BOOT0 strap.
- One placement difference from the wheel: the rail-voltage monitor signals land on **PB0/PB1** here
  instead of the wheel's PA1/PA2 (see the pin table in §7 for why).

**`dash-can.SchDoc`** — copy from the wheel's CAN sheet, but **delete the paddle-shifter circuits
entirely** — the dash has no shift paddles, so that portion of the sheet doesn't apply. Keep the CAN
transceiver chip `U3`, its decoupling capacitors, the protection diode `D2`, and the termination
resistor footprint (populated or not, "DNP" — do not populate — depending on where this board sits on
the bus).

---

## 3. The DAQ front end — reading eight sensors safely (`dash-afe.SchDoc`)

**What it does and why it exists:** the dash reads eight raw 0–5 V analog sensor signals (`AIN1`
through `AIN8`) coming in on connector J2, and needs to hand the MCU numbers it can trust — meaning
scaled into the MCU's input range, filtered against noise, and protected against a sensor wire
shorting to a higher voltage. This same circuit is drawn once as a reusable "device sheet" and then
instantiated eight times, one per channel, because all eight channels are electrically identical.

**How it's built**, per channel:

```
AINn (J2) ──[ Rs 10 kΩ ]──┬── AINn_ADC  (to MCU)
                          ├── Rg 10 kΩ ── GND
                          ├── Cf 100 nF ── GND
                          └── BAV199: upper anode→node, cathode→+3V3
                                      lower cathode→node, anode→GND
```

| Ref | Part / value | Role |
|---|---|---|
| `R9`–`R16` | 10 kΩ 1%, 0402 (a small surface-mount package size) | `Rs`, the series limiter on each channel |
| `R17`–`R24` | 10 kΩ 1%, 0402 | `Rg`, the lower leg of the voltage divider |
| `C24`–`C31` | 100 nF, 0402 | `Cf`, anti-alias / noise filter |
| `D6`–`D13` | **BAV199** dual low-leakage silicon diode, SOT-23 package | the clamp — **not BAT54S** |
| `U6` | TLV9004 quad op-amp, TSSOP-14 | **DNP** (not fitted) by default — 0 Ω jumper resistors bypass it |

**Why BAV199 and not the more common BAT54S — read this one carefully, because it's the kind of
choice someone "cleans up" later without realizing why it's there.** The `Rs`/`Rg` divider on each
channel presents what's called a 5 kΩ Thévenin source at the ADC input — that's electronics shorthand
for "as far as any stray current at that node is concerned, it behaves as if there's a 5 kΩ resistor
between it and the rail." Any tiny leakage current from the clamp diode flowing into that 5 kΩ
effectively looks exactly like real signal to the ADC — the chip can't tell the difference. A BAT54S
(a common Schottky diode, the kind usually reached for as a general-purpose clamp) leaks about 2 µA at
room temperature, which is small, but climbs to roughly 100 µA at 100 °C, which is not small at all.
Do the math: 2 µA × 5 kΩ = 10 mV of offset error when cool, but 100 µA × 5 kΩ = **500 mV** when hot —
and this is a dash board specified to sit in direct sunlight, where 100 °C at the board is a real
possibility, not a stretch. On a 0–2.5 V signal, 500 mV is a **20% error**, and it's the worst kind of
error because it doesn't look like an error — it looks like a plausible sensor reading. The BAV199 is
a low-leakage silicon diode built for exactly this job: it leaks about 3 picoamps (millions of times
less), so the same math gives an offset too small to matter. Its one downside — a slightly higher
forward voltage drop when it's actually conducting during a fault — doesn't cost anything, because it
only conducts when something has already gone wrong.

**Scaling:** the resistor divider is a divide-by-2, so a 0–5 V sensor signal becomes 0–2.5 V at the
ADC pin, leaving 0.8 V of headroom below the MCU's 3.3 V reference in case a sensor slightly
overshoots. The RC filter formed by `Rg` and `Cf` has a corner frequency of about 1/(2π × 5 kΩ ×
100 nF) ≈ **320 Hz** — plenty fast for temperature and pressure signals, which change slowly; if a
future channel needs to track something fast, `Cf` is the value to retune, per channel.

**Fault behaviour:** if a sensor wire is accidentally shorted to the car's 12 V rail, the current that
flows into the ADC pin works out to (12 − 3.3 − 0.7) / 10 kΩ ≈ **0.8 mA** — comfortably inside the
STM32's own internal protection-diode limit of ±5 mA. In other words, the 10 kΩ series resistor alone
already protects the MCU; the external BAV199 clamp is a second layer of protection ("belt and
braces"), which is exactly why it must not be allowed to cost accuracy the way a leakier part would.

**Grounding:** all eight channels' `Rg` and `Cf` return paths go to a dedicated analog ground pour
(a solid copper fill reserved for these returns), which ties back to the main board ground at **one
single point**. Do not let servo current or other digital switching current share that return path —
this is the same "shared ground return injects noise" problem covered next for the servo signals, just
applied to the analog side instead.

---

## 4. The ARB servo outputs — two channels (`dash-servo.SchDoc`)

**What it does and why it exists:** the dash drives two servo motors that adjust the car's adjustable
anti-roll bars (ARBs — front and rear). The driver dials the setpoint in from the wheel using existing
encoder controls; the dash doesn't need any new CAN messages or new firmware protocol to hear it — the
wheel already broadcasts its encoder positions on the bus as voltages on CAN ID `0x2C1`, and because
CAN is a shared, multi-master bus, the dash simply listens to that same broadcast the same way the
ECU does. The dash converts the value it hears into a PWM pulse width using
`pulse_us = PULSE_MIN + (avi_raw / 4095) × (PULSE_MAX − PULSE_MIN)`, where `PULSE_MIN`/`PULSE_MAX` are
configured limits per channel — and those limits *are* the firmware end-stops referenced below. Servo
position feedback comes back in on `AIN7` and `AIN8` of the DAQ front end described in §3 — no extra
hardware needed for that either.

**How it's built**, per channel:

```
PB6 / PB7 (3.3 V PWM) ── U7 74AHCT2G125 (VCC = +5V) ── R(100 Ω) ──┬── J1.5 / J1.6
                                                                  └── SMAJ5.0A ── GND
```

| Ref | Part / value | Connections |
|---|---|---|
| `U7` | **74AHCT2G125**, VSSOP-8 package, dual logic buffer | `VCC` → `+5V`, `GND` → `GND`, both `/OE` (output enable, active low) tied to `GND` so it's always enabled. `A1` ← `SERVO1_PWM_3V3` (PB6), `A2` ← `SERVO2_PWM_3V3` (PB7); `Y1` → `NET_SV1`, `Y2` → `NET_SV2` |
| `C32` | 100 nF, 0402 | `+5V` → `GND` decoupling cap at `U7` |
| `R25`, `R26` | 100 Ω 1%, 0805 | `NET_SV1` → `SERVO1_PWM`, `NET_SV2` → `SERVO2_PWM`; placed right at the buffer output |
| `D14`, `D15` | **SMAJ5.0A** (a TVS diode — a component that clamps a voltage spike to protect what's downstream), SMA package | one per channel, `SERVOn_PWM` → `GND`, placed **at the connector** |

**Why the 74AHCT2G125 buffer is required, not just a nice-to-have:** the MCU's PWM output is 3.3 V
logic, but servos are built to expect roughly 5 V pulses. The 74AHCT logic family has an input
threshold (`V_IH`, the minimum voltage read as a logic "high") of 2.0 V, which a 3.3 V signal clears
legitimately — not marginally — while the chip's output swings up to the 5 V it's powered from. That
translation is what makes the pulse a servo will actually recognize.

**Why the 100 Ω resistor matters:** it's the fault limiter. If a servo signal wire gets shorted to
12 V (a real possibility on a harness that runs out to the suspension), the 100 Ω limits the resulting
current to (12 − 5) / 100 ≈ **70 mA** flowing into the TVS clamp, instead of a much larger current that
could destroy the buffer chip. As a side benefit, it also damps signal ringing on what can be a
multi-metre cable run out to the suspension.

**What must NOT be on this sheet, and why: servo power never crosses this board.** Two high-torque ARB
servos draw roughly **10 A stall current** (the current a servo motor draws when it's pushing against
something and physically can't turn) and about **74 W** combined — against a DTM-series connector pin
rating of only 7.5 A. That current is simply not allowed to pass through this PCB or its connectors;
it would exceed what the board and connector pins are rated for. Instead, servo power comes from a
completely separate 12 V → 7.4 V "BEC" branch (battery eliminator circuit — a small dedicated
regulator) built into the wiring harness, with its own independent fuse. That branch's ground **must**
be star-connected back to the dash's `GND` — meaning it returns to one single common point rather than
looping through the board somewhere — or the PWM signal's reference voltage will float and drift, and
the pulse widths the servos see will jitter. If servo power conditioning is ever brought onto a PCB, it
belongs on a future dedicated servo power conditioning board — not this one.

### 4.1 Why the servo signals live on connector J1, not the DAQ connector J2

This is deliberate, and it's exactly the kind of decision that looks like harmless tidying to someone
who didn't see the reasoning: **the servo PWM outputs are wired to J1 (the power/CAN connector),
not J2 (the DAQ connector) — do not move them.** J2's ground pins carry the return current for all
eight analog sensor channels. Any current that flows through a shared ground return shows up as offset
error on *every single DAQ channel* sharing that return — the same physics as the analog grounding
rule in §3, but for a much bigger current source. A PWM signal also needs to reference the same ground
its receiving end (the servo) actually uses, and the servo's BEC ground stars back to the dash's power
ground, not the DAQ ground. Putting the servo signals on J1 keeps them referenced to the right ground
and physically separate from the sensitive analog returns.

### 4.2 The four ARB servo safety rules

These come from the firmware side of the design, and the source material is explicit that they are
**mandatory requirements, not preferences** — the kind of thing a reviewer should refuse to sign off
without. A servo is a powerful, unforgiving actuator bolted to a suspension linkage; get any of these
wrong and the likely outcome is a burned-out servo or a bent link, not just a bug.

1. **Hold the last commanded position if the CAN bus goes quiet — never spring back to a default or
   centre position.** If the dash stops hearing setpoint messages from the wheel, the correct and only
   acceptable behaviour is "fail-frozen": stay exactly where it last was told to be. Springing to a
   default position (like centre) the instant communication drops would move the anti-roll bar on its
   own, with no driver input, while the car is potentially still moving — that is not an acceptable
   failure mode under any circumstance.
2. **Software end-stops must exist and be enforced before a servo is ever bolted to a linkage.** The
   commanded pulse width must always be clamped to a per-channel minimum and maximum. A servo that is
   driven past the mechanical limit of its linkage will keep drawing full stall current trying to
   reach a position it physically cannot reach, and that is the single most likely way to destroy one
   — so the software clamp has to be in place and correct *before* the mechanical linkage is
   connected, not added afterward as a fix.
3. **Slew-limit the output** so the servo cannot travel its full range faster than about 2 seconds.
   This exists specifically so that a quick spin of the setpoint knob on the wheel can't slam the
   anti-roll bar linkage from one extreme to the other instantly.
4. **Run a divergence alarm.** If the commanded position and the measured feedback position (coming
   back on `AIN7`/`AIN8`) disagree by more than a set threshold for longer than 500 ms, the dash must
   raise a warning and log it. This is the mechanism that catches a seized linkage, a stripped drive
   spline, or a dead servo — none of which announce themselves any other way.

A fifth, related requirement from the same list, worth keeping in mind even though it's a hardware
rule rather than a firmware one: the **servo power supply must be independently fused and must be
killable without taking the rest of the dash down** — so a servo fault can be isolated without losing
the display, DAQ, or CAN link.

**Looking ahead:** the external BEC described above is deliberately treated as a harness component for
now, but the plan is eventually to build a small dedicated servo power conditioning board — a fourth
board in this project family — to properly absorb servo inrush and stall transients, add per-channel
fusing and current sensing, and keep that dirty load off the 12 V rail that the dash and ECU currently
share with it. Until that board exists, the BEC's selection, fusing, and grounding are still real
design deliverables that need the same level of review as anything on a PCB — "temporary" is not the
same as "unreviewed."

---

## 5. The display — Riverdi RVT50HQBNWN00 (`dash-display.SchDoc`)

**What it does and why it exists:** this is the 5-inch colour display the driver reads live data from.
It connects through `J4`, a 20-pin, 0.5 mm pitch ZIF connector (a "zero insertion force" connector that
clamps a flat cable in place rather than requiring pins to be pushed in), using the matched flat cable
`FFC0520150`. The pinout below has been checked directly against the Riverdi datasheet, revision 1.7.

| Pin | Signal | Connect to |
|---|---|---|
| 1 | VDD | `+3V3` (module logic supply: 98 mA typical, 384 mA absolute maximum) |
| 2 | GND | `GND` |
| 3 | SPI_SCLK | `EVE_SCK` (PA5) |
| 4 | MISO / IO1 | `EVE_MISO` (PA6) |
| 5 | MOSI / IO0 | `EVE_MOSI` (PA7) |
| 6 | CS | `EVE_CS` (PA4) |
| 7 | INT | `EVE_INT` (PB4) — active low, **internally pulled up 47 kΩ inside the display module**, add nothing external |
| 8 | RST/PD | `EVE_PDN` (PB3) — active low, **internally pulled up 47 kΩ** |
| 9 | GPIO.0 | spare — bring out to a test point |
| 10 | DISP_AUDIO | leave open (no speaker fitted) |
| 11 | GPIO.1 / IO2 | `EVE_IO2` — route it; firmware may later upgrade the interface to QSPI |
| 12 | GPIO.2 / IO3 | `EVE_IO3` — same reasoning |
| 13–16 | NC (not connected) | leave open |
| 17, 18 | **BLVDD** | **`+5V` — both pins.** The backlight is a separate power input from the logic supply |
| 19, 20 | **BLGND** | `GND` — both pins |

| Ref | Value | Connection |
|---|---|---|
| `C33` | 22 µF, 16 V, X7R ceramic, 0805 | `+3V3` → `GND` at J4 |
| `C34` | 1 µF, 0603 | `+3V3` → `GND` at J4 |
| `C35` | 100 nF, 0402 | `+3V3` → `GND` at J4 |
| `C35b` | 22 µF, 16 V, 0805 | `+5V` → `GND` at pins 17/18 — the backlight is the switching load here, so it needs its own bulk capacitance separate from the logic supply's |
| `R27`, `R28` | 22 Ω, 0402 | in series on `EVE_SCK` and `EVE_MOSI`, placed at the MCU end, to control signal ringing on that trace |

**Why the backlight is powered from the 5 V rail and not 3.3 V:** unlike the display's logic supply
(`VDD`, on 3.3 V), the backlight (`BLVDD`) is a completely separate power input, specified at
3.1 V / **5.0 V typical** / 5.5 V, and it draws **353 mA at 5 V** for full brightness. It has to come
from `+5V`. The reason is that the backlight driver inside the module is a constant-current design —
meaning it regulates to a fixed brightness by drawing whatever current that brightness needs at
whatever voltage it's given. Feed it 3.3 V instead of 5 V, and it doesn't get dimmer — it draws *more*
current (657 mA, calculated for the same brightness) to compensate, which is worse for the power
budget in every way. On the logic side, the display's inputs accept `V_IH` (logic-high threshold)
2.0 V and `V_IL` (logic-low threshold) 0.8 V, so the MCU's 3.3 V GPIO pins can drive it directly with
no extra circuitry needed there.

---

## 6. Buttons, the LED strip, and the connectors

### 6.1 Local buttons (optional, 3 off)

The dash can optionally carry three local pushbuttons, built with the exact same button-conditioning
circuit as the wheel: `SW1`–`SW3` (C&K KSC4 switches) feed nets `BTN1`–`BTN3`, through `R29`–`R31`
(1 kΩ series resistors), with `R32`–`R34` (10 kΩ pull-ups) and `C36`–`C38` (100 nF) for debounce
filtering.

### 6.2 Alarm LED strip (optional)

`U8`, a 74AHCT1G125 logic buffer, can drive an off-board LED strip from net `LED_DATA_3V3` (PA8). It
is **DNP (do not populate) by default**, but the footprint and a 3-pin JST-GH connector (carrying
`+5V`, the data line, and `GND`) should still be laid out so this can be fitted later without a board
respin.

### 6.3 J1 — power, CAN bus, and servo signal (DTM04-12P connector, positions 1–6)

| Pin | Net |
|---|---|
| 1 | `+12V_IN` |
| 2 | `GND` — **also the reference ground for the servo PWM signals** |
| 3 | `CAN_H` |
| 4 | `CAN_L` |
| 5 | `SERVO1_PWM` |
| 6 | `SERVO2_PWM` |

The silkscreen (the printed text on the board itself) at J1 should read: **"+12V IN — FUSED 2A"** and
**"SERVO SIGNAL ONLY — NO SERVO POWER"** — a direct, physical reminder of the rule from §4.2 that
servo power current must never be routed onto this connector or this board.

### 6.4 J2 — DAQ connector (DTM04-12P connector)

| Pin | Net |
|---|---|
| 1–6 | `AIN1` … `AIN6` |
| 7, 8 | `AIN7`, `AIN8` — ARB position feedback (front, rear) |
| 9 | `+5V_SENS` |
| 10–12 | `GND_SENS` (three separate return pins) |

The `+5V_SENS` branch that feeds pin 9 works like this: `+5V` → `F2` (a 200 mA polyfuse, 0805 package)
→ `+5V_SENS`, with `D2b` (an **SMBJ5.0A** TVS diode) from `+5V_SENS` to `GND` right at the connector.
The silkscreen at J2 should read **"0–5 V MAX"**.

---

## 7. Power budget — will the regulators actually cope?

| Load | Rail | Current |
|---|---|---|
| Display backlight (`BLVDD`) at 100% brightness | 5 V | 0.353 A |
| Display module logic (`VDD`) | 3.3 V | 0.098 A typical (0.384 A absolute max, only reached with audio, and no speaker is fitted) |
| MCU + CAN + analog front end | 3.3 V | 0.10 A |
| Sensor excitation (`+5V_SENS`) | 5 V | 0.20 A |
| Servo buffers | 5 V | under 0.01 A |

Adding those up: the 3.3 V rail totals roughly **0.5 A**, supplied here by the **AP63203WU-7 buck
converter** (fed from `+5V`) — the dash's 3.3 V rail is not an LDO. Because a buck trades voltage for
current rather than just burning off the difference as heat, that 0.5 A at 3.3 V reflects back as
roughly **0.37 A** drawn from the 5 V rail (0.5 × 3.3/5, then divided by the buck's roughly 90%
efficiency). So the 5 V rail carries about **0.6 A** of direct load plus that **0.37 A** reflected,
for about **1.0 A** total against the LMR36015's 1.5 A rating — comfortable. The 12 V input current
works out to roughly **0.46 A**, well inside the 2 A polyfuse chosen for `F1`.

The two boards deliberately use different parts here, and that's not an inconsistency to "harmonise"
away: the wheel's 3.3 V load is only around 100 mA, so an LDO is fine there (0.17 W dissipated), while
the dash's roughly 0.5 A load would dissipate 0.85 W in an LDO — enough to matter in a small package —
so the dash keeps a switching converter (the AP63203) on this rail instead.

---

## 8. MCU pin assignment

This table has been checked against the STM32G474 datasheet's alternate-function table (the official
reference for which pin can do which job).

| Pin | Net | Why |
|---|---|---|
| PA4 / PA5 / PA6 / PA7 | `EVE_CS` / `EVE_SCK` / `EVE_MISO` / `EVE_MOSI` | a complete, genuine SPI1 hardware peripheral set (chip-select / clock / data-in / data-out) |
| PB3 / PB4 | `EVE_PDN` / `EVE_INT` | plain digital I/O pins |
| PA0–PA3, PC0–PC3 | `AIN1_ADC` … `AIN8_ADC` | all eight are reachable by ADC1 and/or ADC2, so one scan sequence (or ADC1+ADC2 running together) covers all eight — no channel is stuck on an ADC instance that can't be used together with the rest. Exact channel numbers are a CubeMX (ST's chip-configuration tool) detail, not a schematic one |
| PB0 / PB1 | `V12_SENSE` / `V5_SENSE` | both ADC1/ADC2 capable |
| **PB6 / PB7** | `SERVO1_PWM_3V3` / `SERVO2_PWM_3V3` | **TIM4_CH1 / TIM4_CH2** — both servo channels share one hardware timer, keeping their timebases in sync |
| PA8 | `LED_DATA_3V3` | TIM1_CH1 — nothing else on this board competes for TIM1 |
| PC8–PC10 | `BTN1`–`BTN3` | plain digital input |
| PB8 / PB9 | `CAN_RX` / `CAN_TX` | FDCAN1 — **fit nothing on BOOT0**, per §2 |
| PA11 / PA12 | `USB_DM` / `USB_DP` | USB full-speed |
| PA13 / PA14 | `SWDIO` / `SWCLK` | debug/programming interface |
| PA9 / PA10 | `DBG_TX` / `DBG_RX` | USART1 |
| **PF0 / PF1** | `OSC_IN` / `OSC_OUT` | the HSE crystal, physically pins 5/6 on the LQFP-64 package — required for correct CAN bit timing (§2) |

**One cross-board note worth remembering:** PB6/PB7 carry the servo signals on the dash, but on the
wheel board those same pin numbers are used for `ENC3_A`/`ENC3_B` (an encoder input). Likewise PB3/PB4
are display control here but `ENC4_B`/`PADDLE_UP_SNS` on the wheel. This is not a conflict — they're
different boards — but because both boards share one firmware codebase, the code must select behaviour
based on a board-personality setting (a compile-time `#define`) and must never assume a given pin
number means the same thing on both boards.

---

## 9. Open items — nothing here blocks starting the schematic

| Item | Why it matters | How to close it |
|---|---|---|
| LMR36015 exact ordered variant | the 400 kHz version of this part needs a 15 µH inductor and 3× 22 µF output capacitors — different from the values used elsewhere | Look up the ordering table for whichever exact variant gets purchased |
| SMAJ5.0A / SMBJ5.0A TVS diode parameters | considered low risk because their standoff voltage clearly exceeds the 5 V rail, so they cannot conduct in normal operation — the rule is standoff ≥ rail, clamp ≤ downstream absolute max (`engineering-rigor.md` rule 5) | Worth a quick confirming check |

None of these are blockers — they're flagged so nobody forgets to close them before the board is
finalized, not because the design can't move forward.

---

## Things I could not explain simply

- The exact reasoning for why the rail-monitor signals land on PB0/PB1 on the dash instead of the
  wheel's PA1/PA2 is stated as a fact (source §2) but the underlying "why this specific pin swap" isn't
  spelled out in the source material beyond "see §8" (the pin table) — the pin table confirms both are
  valid ADC-capable pins, but doesn't say what made PB0/PB1 the better choice on this particular board
  layout.
- The note that R27/R28 (the 22 Ω series resistors on the display's SPI clock and data lines) control
  ringing "at 30 MHz" is stated in the source without saying whether that's the SPI clock's actual
  operating frequency or a signal-edge/trace-length calculation — the display's real SPI ceiling is
  much lower (2 MHz, per the datasheet-verification notes), so 30 MHz likely refers to a signal-edge
  rate rather than the clock frequency itself, but the source doesn't spell out which.
