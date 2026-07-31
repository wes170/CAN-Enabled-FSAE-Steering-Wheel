# Datasheet Verification Record

> **Permanent memory file.** Created 2026-07 after an audit question exposed that the original
> schematic instructions were written largely **from memory rather than from datasheets**. This file
> records, per part: what the datasheet actually says, whether the design complies, and what was
> corrected. A part is not `VERIFIED` until someone has read its datasheet and written the finding here.
>
> **Status legend:** `VERIFIED` = datasheet read, design checked · `PARTIAL` = some parameters checked ·
> `UNVERIFIED` = not yet read, treat every claim about it as provisional.

## Summary of status

| Part | Status | Outcome |
|---|---|---|
| STM32G474RET6 | **PARTIAL — see §1.4b** | **6 defects found** (4 pin-map, missing clock source, wrong BOOT0 option-bit name). See §1. **Step 2 found 4 more: UCPD dead-battery pull-down on PB6/PB4, ADC channels off by one, paddle pins over abs-max on a TVS clamp, stale authoritative pin table. See §9** |
| Sharp LS013B7DH05 | **PARTIAL** | **1 defect found (EXTMODE omitted).** See §2 |
| TJA1051T/3 | **VERIFIED** | Compliant as designed. See §3 |
| Haltech CAN broadcast protocol | VERIFIED | Read in full; §2.3 table transcribed from it |
| Blink PKP2600SI CANopen | VERIFIED | Read in full; keypad frames transcribed from it |
| WS2812B-2020 | **UNVERIFIED** | Datasheet is image-only, not text-extractable. See §4 |
| AP63205 / AP63203 | **VERIFIED** | **CRITICAL defect — 35 V abs max cannot face a vehicle battery.** See §5. AP63203 datasheet fully read in Step 2: 35 V confirmed, L2 = 4.7 µH closed, **missing bootstrap capacitor found** — see §9 defects 8.8/8.8b |
| SMBJ33A vs buck abs-max | **VERIFIED** | **CRITICAL — TVS clamps 18 V above what the wheel buck survives.** See §5 |
| LMR33630 | **PARTIAL** | **REJECTED** on abs-max grounds (§5). Step 2 found it still specified as live in the dash capture instructions — see §9 defect 8.4 |
| **AMS1117-3.3** | **VERIFIED** | **Defect — datasheet requires a tantalum output cap; BOM specifies ceramic.** See §5C |
| **DMP3056L** | **VERIFIED** | Adequate; −30 V V_DSS is thin against a reverse jump start. See §5D |
| **BAT54S → BAV199** | **VERIFIED** | **Defect — Schottky leakage corrupts the DAQ channels.** See §5A |
| **Riverdi RVT50HQBNWN00** | **VERIFIED** | **Defect — backlight is a separate 5 V rail, 353 mA not 1.2 A.** See §5B |
| **Dash pin map** | **VERIFIED** | Sound; BOOT0 handled by option bit. **ADC question closed: all 8 DAQ pins reach ADC1/ADC2**, so one scan sequence covers them. See §6 |
| PEC09 / PEC11H / KSC4 | **PARTIAL** | Distributor parametric data + conditioning analysed (§5E); mechanical drawings and bounce duration still unread |
| **LMR36015 / AP2112K-3.3** | **VERIFIED** | Final selections, both closed. See §6A |
| **PESD2CAN / USBLC6-2SC6** | **VERIFIED** | Compliant; margins computed. See §6A |
| SMAJ24CA / SMAJ5.0A / SMBJ5.0A | **UNVERIFIED** | Low risk (standoff clearly above their rails); §7 |


---

## 1. STM32G474RET6 — VERIFIED, three defects found

Source: ST datasheet **DS12288 / stm32g474cb.pdf**, Table 13 (Alternate function) and the pin
definition table, extracted and read directly.

### Defect 1.1 — Only 2 of 6 encoder pairs could have worked (CRITICAL)

STM32 hardware quadrature decoding requires **TIMx_CH1 *and* TIMx_CH2 of the same timer**. The
original map assigned arbitrary adjacent GPIO pairs and claimed "TIM encoder mode ×4". Checked
against the real AF table:

| Original pair | Reality | Verdict |
|---|---|---|
| PC0 / PC1 | TIM1_CH1 / TIM1_CH2 | ✅ valid (by luck) |
| PC2 / PC3 | TIM1_**CH3** / TIM1_**CH4** | ❌ encoder mode uses CH1/CH2 only |
| PC4 / PC5 | TIM1_ETR / TIM15_BKIN | ❌ not a channel pair at all |
| PC6 / PC7 | TIM3_CH1 / TIM3_CH2 | ✅ valid |
| PB0 / PB1 | TIM3_CH3 / TIM3_CH4 | ❌ wrong channels, and TIM3 already taken |
| PB2 / PB10 | TIM5_CH1 / TIM2_CH3 | ❌ different timers |

Four of six encoders would have had no hardware decoder. On a wheel where a flicked encoder is
exactly the case you must not miss counts on, that is a functional failure, not a nuisance.

### Defect 1.2 — LED data pin collided with an encoder timer

`LED_DATA` was assigned **PA8 = TIM1_CH1**, while encoder 1 used **PC0 = TIM1_CH1**. The same timer
channel cannot do both. One of the two would have been silently dropped at CubeMX configuration.

### Defect 1.3 — CAN_RX sits on the BOOT0 pin (CRITICAL, and unavoidable on this package)

`FDCAN1` on LQFP-64 is available **only** on `PA11/PA12` or `PB8/PB9` (PD0's FDCAN1_RX is not bonded
on LQFP-64 — confirmed "–" in the LQFP-64 column). PA11/PA12 are `USB_DM`/`USB_DP`, which the sim
variant and DFU need. **Therefore CAN must use PB8/PB9 — and PB8 is `PB8-BOOT0`.**

Why this matters: an idle CAN bus is **recessive = RXD HIGH**. If BOOT0 is taken from the pin, the
MCU samples BOOT0 = 1 at every power-on with a live bus and jumps to the system bootloader instead of
the application. The wheel would simply never start in the car, while working perfectly on the bench
with the bus unplugged — one of the nastiest failure signatures possible.

**Required mitigation (must be in the build procedure, not just the schematic):**
1. Set the option bits so BOOT0 comes from the **option bits, not the pin**: on STM32G4 that is **`nSWBOOT0` (FLASH_OPTR[26]) = 0** so BOOT0 is taken from the option bit rather than PB8, **and `nBOOT0` (FLASH_OPTR[27]) = 1** to boot main flash. Set them with **STM32CubeProgrammer** — boot mode is latched during the reset sequence, so firmware cannot fix this after the fact,
   as part of first flashing. Verify it on every board.
2. **Delete the "10 kΩ BOOT0 pulldown + test point" from the original instructions** — that circuit
   fights the CAN transceiver's RXD output and is actively harmful here.
3. DFU entry uses the USB DFU path or an SWD-triggered jump, **not** a BOOT0 strap.
4. A full chip erase can restore the factory option-bit state, so **re-check `nSWBOOT0`/`nBOOT0` after any
   mass erase.** Put it in the bring-up log.

### Defect 1.8 — paddle sense taps expose a 3.3 V pin to the full paddle-line voltage (MAJOR)

Found while verifying the SMAJ TVS parts. Two compounding errors:

**(a) The pin type was wrong in my own validation table.** I recorded the paddle sense pins as
"5 V-tolerant FT". They are **`TT_a`** — 3.3 V-tolerant analog-capable pins whose **absolute maximum
input is 4.0 V** (Table 14). Verified directly from the pin definition table.

**(b) There is no lower divider resistor.** The circuit is
`PADDLE_UP → 100 kΩ → PADDLE_UP_SNS → 1 nF to GND`. A capacitor is not a DC path, so once it charges
**the pin sits at the full paddle-line DC voltage**, limited only by the MCU's internal ESD structure
— and per defect 1.7 positive injection is not a permitted mode on these pins.

The paddle lines are pulled up by the **Nexus digital inputs**, typically to 5 V or 12 V. Either
exceeds the 4.0 V limit; 12 V exceeds it by 3×.

**Fix — read the tap as an ADC input, not a GPIO.** PB0 and PB1 are already ADC-capable
(`ADC1_IN15`/`ADC3_IN12` and `ADC1_IN12`/`ADC3_IN1`), which resolves a problem a plain divider
cannot: a divider sized so 12 V lands safely under 4.0 V puts a 5 V pull-up at ~1.4 V, **below the
2.31 V logic-high threshold** — so no single divider serves both possible ECU pull-up voltages as a
digital input.

| Change | Value |
|---|---|
| Series resistor `R6`/`R7` | **150 kΩ** (was 100 kΩ) |
| **New** lower resistor to GND | **39 kΩ** |
| Filter cap `C14`/`C15` | 1 nF, unchanged |
| Pin voltage at **16 V** (alternator max) | 16 × 39/189 = **3.30 V** ✅ (0.70 V under the limit) at the *charging-system* maximum, not just nominal 12 V |
| Pin voltage at 5 V pull-up | 5 × 39/189 = **1.03 V** ✅ readable as analog; 12 V gives 2.48 V |
| Load on the ECU line | 189 kΩ → **85 µA at 16 V**, 63 µA at 12 V ✅ still negligible |

**Sized for 16 V, not 12 V.** A first cut at 150 kΩ/56 kΩ put a 12 V line at a comfortable 3.26 V — but a running 12 V system charges at 14.4 V and can sit higher, and 16 V through that divider gives **4.35 V, over the 4.0 V limit**. The divider must be sized against the charging-system maximum, not the nominal rail. 150 kΩ/39 kΩ holds 16 V to 3.30 V.

Firmware thresholds in software (e.g. >0.5 V = pressed) and works with either pull-up voltage —
and usefully, the measured level *tells you* which one the ECU uses, closing that question by
observation rather than by asking.

**Why this hid:** the paddle path is described everywhere as "pure copper passthrough", which is true
and is the safety-critical property. The sense tap is a footnote to that story, so it never got the
scrutiny an input pin deserves. **A monitoring tap is still an input.**

### Defect 1.7 — the DAQ clamp is primary protection, not "belt-and-braces" (MAJOR)

Closing the injected-current item produced a worse answer than expected. Read from the STM32G474
datasheet, **Table 15 (current characteristics)** and **Table 14 (voltage characteristics)**:

| Spec | Actual value | What the docs claimed |
|---|---|---|
| `IINJ(PIN)` on FT/TT/NRST | **−5 / 0 mA** | "±5 mA" ❌ |
| Positive injection | *"**not possible** on these I/Os"* (note 3) | assumed an internal clamp would absorb 0.8 mA ❌ |
| `Σ\|IINJ(PIN)\|` | ±25 mA total | not stated |
| **Input voltage, TT_xx pins** | **VSS−0.3 to 4.0 V** absolute max | assumed VDD+0.3 = 3.6 V |

**The injection limit is asymmetric.** −5 mA applies to *negative* injection (pin driven below VSS).
For positive overvoltage the datasheet does not offer an injection allowance at all — it simply
states the input voltage must not exceed **4.0 V**.

**Consequence: the AFE reasoning was wrong in a way that matters.** The docs said a 12 V sensor fault
injects 0.8 mA into the MCU's internal diodes, "comfortably inside the ±5 mA limit", and concluded
the external clamp was *belt-and-braces*. There is no sanctioned positive-injection path, so:

> **The BAV199 clamp is the primary and only protection against a positive overvoltage fault on a
> DAQ channel.** It is load-bearing. It must never be depopulated, and it must hold the pin below
> 4.0 V.

**Does it? — marginally, yes.** The 10 kΩ series resistor limits fault current to
`(12 − 3.3 − Vf)/10 kΩ ≈ 0.8 mA`. At **0.8 mA**, a small-signal silicon diode's forward drop is about
**0.55–0.6 V**, not the 0.7–1.0 V quoted at rated current — so the pin sits near **3.9 V** against a
**4.0 V** absolute maximum. It passes, with roughly **0.1 V of margin**, which is far thinner than
rigor rule 5 wants.

**[OPEN — resolve in the Step 2 design pass, do not hack a value now].** The tension is real and
deserves proper treatment rather than a quick substitution:
- A Schottky clamp (≈0.3 V at this current) would give 3.6 V and comfortable margin, but that is the
  BAT54S whose leakage corrupts the measurement (§5A) — though note §5A used the 100 µA figure
  specified at V_R = 30 V, and Schottky leakage falls steeply with reverse voltage, so the real
  penalty at V_R ≈ 3.3 V needs reading off the curve rather than the headline number.
- Raising the series resistor lowers fault current and V_f further, but raises the Thévenin source
  impedance the ADC has to settle against.
- Changing the divider ratio bounds the fault inherently but costs ADC resolution.

Related gap noticed while doing this: **the `AIN` lines have no TVS at the connector**, unlike every
other line that leaves a board. Sensor wiring runs the length of the car. Worth deciding deliberately
in Step 2 rather than by omission.

### Defect 1.6 — the BOOT0 mitigation named a bit that does not exist on this family (CRITICAL)

The mitigation for defect 1.3 said: *"set the option bits so BOOT0 comes from the `nBOOT0` option
bit, not the pin (`nBOOT_SEL = 1`)."* **`nBOOT_SEL` is not an STM32G4 bit.** It belongs to other
families (G0, U0, L5). I wrote it from memory and flagged it in §1.4b as *"the one that matters,
because the whole BOOT0 fix rests on them"* — that flag was correct, and the claim was wrong.

**The actual STM32G4 option bytes**, confirmed by an ST staff answer on the identical case
(STM32G431, PB8 wanted as a normal pin):

| Bit | Register | Set to | Effect |
|---|---|---|---|
| **`nSWBOOT0`** | FLASH_OPTR[26] | **0** | BOOT0 is taken from the option bit, **not** from the PB8 pin |
| **`nBOOT0`** | FLASH_OPTR[27] | **1** | Boot from main flash |

Two further points the original text missed entirely:

- **These must be set with STM32CubeProgrammer.** Boot mode is latched during the reset sequence,
  so there is **no firmware workaround** for a mis-provisioned board — you cannot fix it in `main()`.
- A **mass erase restores `nSWBOOT0 = 1`**, silently re-arming the failure. The re-check after erase
  was already in the docs; now it names the right bit to re-check.

**Why this was the worst possible place for a wrong name.** Defect 1.3 is unavoidable on this
package — CAN *must* live on PB8/PB9 once USB claims PA11/PA12 — so the option-byte setting is not a
nicety, it is the entire reason the board boots in a car. A student following the old instruction
would have searched CubeProgrammer for `nBOOT_SEL` on a G4, not found it, and either guessed or given
up. The failure would then present exactly as defect 1.3 describes: perfect on the bench, dead on
a live bus.

Corrected in all seven places it appeared: the verification record, both schematic definitions, both
Altium instruction files, `board_config.h`, and all three plain-English guides.

### Defect 1.5 — ENC5 was on a timer that cannot decode encoders (found during firmware work)

Hardware quadrature decoding needs **two** things, and the earlier fix only checked one:

1. the two pins are **CH1 and CH2 of the same timer** — checked in defect 1.1; and
2. **that timer actually implements an encoder interface** — *not* checked.

ENC5 was assigned PB14/PB15 = **TIM15**_CH1/CH2. TIM15 has two channels, so it passed test 1 and
looked correct. But the datasheet §3.24.3 describes TIM15/16/17 as "general-purpose timers with
mid-range features" listing input capture, output compare, PWM and one-pulse mode — **no quadrature
encoder**. By contrast §3.24.3 says of TIM2/3/4/5: "All have independent DMA request generation and
**support quadrature encoders**", and §3.24.5 explicitly lists "Encoder mode" for LPTIM1. The
datasheet states encoder support where it exists, and does not for TIM15.

**Encoder-capable timers on this part: TIM1, TIM2, TIM3, TIM4, TIM5, TIM8, TIM20, and LPTIM1.**

**Fix.** TIM1–TIM5 were already taken by ENC1–ENC4 and ENC6, and TIM8's channel pins collide with
CAN and SWD. **TIM20** — an advanced-control timer, encoder-capable — has `TIM20_CH1` on **PB2** and
`TIM20_CH2` on **PC2**, both bonded on LQFP-64. PB2 was spare; PC2 held `LCD_DISP`, which is a plain
GPIO and moved to **PB13**. ENC5 is now **PB2/PC2 on TIM20**, and PB14/PB15 return to spare.

**Why this survived the earlier pass:** defect 1.1 established the rule "CH1+CH2 of one timer" and I
then applied that rule mechanically to a timer that does not have the feature at all. A rule derived
from one failure became a blind spot for the adjacent failure. Recorded as lesson L22.

### Corrected wheel pin map — every entry justified by the AF table

| Function | Pin | Justification (from Table 13) |
|---|---|---|
| ENC1 A / B | PC0 / PC1 | TIM1_CH1 / TIM1_CH2 |
| ENC2 A / B | PC6 / PC7 | TIM3_CH1 / TIM3_CH2 |
| ENC3 A / B | PB6 / PB7 | TIM4_CH1 / TIM4_CH2 |
| ENC4 A / B | PA15 / PB3 | TIM2_CH1 / TIM2_CH2 |
| ENC5 A / B | **PB2 / PC2** | **TIM20_CH1 / TIM20_CH2** — TIM20 is encoder-capable; TIM15 was not (defect 1.5) |
| ENC6 A / B | PA0 / PC12 | TIM5_CH1 / TIM5_CH2 |
| LED_DATA | PA6 | TIM16_CH1 + DMA — moved off TIM1 (defect 1.2) |
| LCD_SCLK / LCD_SI | PA5 / PA7 | SPI1_SCK / SPI1_MOSI (PB3 alternative is taken by ENC4) |
| LCD_SCS / LCD_DISP / LCD_EXTCOMIN | PA4 / **PB13** / PC3 | plain GPIO; EXTCOMIN is a ~1 Hz software toggle, no timer needed |
| CAN_RX / CAN_TX | PB8 / PB9 | FDCAN1 — **only option left after USB claims PA11/PA12** (defect 1.3) |
| USB_DM / USB_DP | PA11 / PA12 | USB FS |
| SWDIO / SWCLK | PA13 / PA14 | |
| DBG_TX / DBG_RX | PA9 / PA10 | USART1 — free now that TIM1 encoder moved to PC0/PC1 |
| V12_SENSE / V5_SENSE | PA1 / PA2 | ADC1_IN3 / ADC1_IN4 |
| PADDLE_UP/DN_SNS | PB0 / PB1 | GPIO input |
| ENC1–6_SW | PC4, PC5, PC8, PC9, PC10, PC11 | GPIO input |
| BTN1–6 | PC13, PD2, PA3, PB10, PB11, PB12 | GPIO input (PD2 = pin 55, confirmed bonded on LQFP-64) |
| Spare | PA8, PB4, PB5, PB14, PB15 | |

Verified bonded on LQFP-64 (pin numbers from the pin definition table): PC13=2, PC0=8, PC1=9,
PC2=10, PC3=11, PC4=22, PC5=23, PB14=36, PB15=37, PC6=38, PC7=39, PC8=40, PC9=41, PA8=42, PA9=43,
PA15=51, PC10=52, PC11=53, PC12=54, PD2=55, PB3=56, PB4=57, PB5=58.

**Dash pin map is now verified separately in §6** — it fared much better than the wheel's, because it
never attempted six hardware encoder pairs. It does inherit the CAN/BOOT0 issue (defect 1.3).

---

### Defect 1.4 — No clock source was specified at all (CRITICAL)

The schematic definition had **no crystal and no oscillator section**. The MCU would have run on the
internal HSI16 RC, and the datasheet (Table 43) gives its accuracy as:

| Condition | Drift |
|---|---|
| 0 … 85 °C | **−1 % / +1 %** |
| −40 … 125 °C | **−2 % / +1.5 %** |

CAN bit timing tolerates roughly **±0.5 % per node** in practice (about ±1.58 % in the theoretical
best case with ideal sample-point placement), and that budget is **shared with every other node on
the bus** — two nodes each 1 % off are 2 % apart. At 1 Mbit/s on HSI16 the boards would throw
intermittent error frames and bus-off events, worsening as the car heats up. Classic
"perfect on the bench, broken in the car", and it presents as a software bug.

**Corrected:** `Y1` HSE crystal (8 or 16 MHz, ≤50 ppm) on **PF0-OSC_IN (LQFP-64 pin 5)** and
**PF1-OSC_OUT (pin 6)** — both verified bonded on this package — with load caps sized from the chosen
crystal's CL. A 50 ppm part is 0.005 %, a hundred times better than CAN needs, and costs pennies.
USB does not force the decision (the G4 can run crystal-less USB via HSI48 + CRS), but since CAN
requires a crystal anyway, clocking both from the HSE removes the question entirely.

### 1.4b What was NOT read from the STM32 datasheet

Honest scope of the MCU verification. **Read and checked:** Table 13 alternate functions, the pin
definition table (LQFP-64 bonding and pin numbers), FDCAN1 pin options, the `PB8-BOOT0` naming, ADC
instance availability per pin, HSI16 accuracy (Table 43), and OSC pin availability.

**Not read — these values appear in the docs but came from memory and are still unverified:**

| Claim | Where it is used | Risk if wrong |
|---|---|---|
| ~~±5 mA pin injection limit~~ | The DAQ fault analysis | ✅ **CLOSED — and it was WRONG.** Actual: **−5/0 mA**, positive injection not permitted; TT pin input abs max **4.0 V**. See defect 1.7 |
| ~0.10 A MCU run current at 170 MHz | Both boards' power budgets | Budgets have large margin; low risk |
| ADC max external source impedance / sample-time table | Justifies omitting the op-amp buffer at 5 kΩ | Could force the DNP TLV9004 to be fitted |
| ~~Six VDD pins on LQFP-64~~ | Decoupling count | ✅ **CLOSED.** LQFP-64 power pins are **VBAT 1, VSS 15/31/47, VDD 16/32/48, VSSA 27, VREF+ 28, VDDA 29** — so **three** VDD pins, not six. Place 100 nF at each of VDD 16/32/48, at VDDA, and at VBAT, plus the bulk cap |
| ~~`nSWBOOT0 = 0` **and** `nBOOT0 = 1`~~ | The BOOT0 mitigation (defect 1.3) | ✅ **CLOSED — and it was WRONG. See defect 1.6.** The G4 bits are `nSWBOOT0 = 0` + `nBOOT0 = 1` |
| Absolute maximum ratings, VDDA sequencing | General | Standard practice covers it |

**That flag was justified — the claim was wrong. See defect 1.6.**

---

## 2. Sharp LS013B7DH05 — PARTIAL, one defect found

Source: Sharp spec sheet **LD-27503A**, §4 Table 4-1, read directly.

**Defect 2.1 — `EXTMODE` (pin 8) was omitted from the schematic instructions.** The real terminal
list is 10 pins: 1 SCLK, 2 SI, 3 SCS, 4 EXTCOMIN, 5 DISP, 6 VDDA, 7 VDD, 8 **EXTMODE**, 9 VSS,
10 VSSA. The original instruction listed seven signals and never mentioned EXTMODE.

EXTMODE selects the COM-inversion source: `Hi` enables the hardware EXTCOMIN pin, `Lo` uses the
software serial flag. COM inversion is what prevents a DC bias accumulating across the liquid
crystal, so leaving pin 8 floating gives undefined inversion behaviour and risks permanent image
sticking. **Corrected:** strap EXTMODE to `+3V3` via a 0 Ω link (DNP pulldown alternative), matching
the hardware-EXTCOMIN scheme the firmware uses. Already applied to
`wheel-pcb-altium-instructions.md` §3.6.

Also confirmed from the spec: VDD and VDDA are separate supplies with separate returns (VSS/VSSA),
and the spec explicitly requires low VDD–GND / VDDA–GND impedance in use, which justifies the
decoupling at the connector.

**Still unverified on this part:** FPC contact side (top vs bottom — a mirrored pinout if wrong),
SCS polarity and setup/hold timing, VDD voltage range, EXTCOMIN frequency requirement.

---

## 3. TJA1051T/3 — VERIFIED, compliant

Source: NXP TJA1051 datasheet, pin description table and characteristics, read directly.

| Datasheet says | Design does | Verdict |
|---|---|---|
| Pin 1 TXD, 2 GND, 3 VCC, 4 RXD, **5 VIO** (T/3 and TK/3 only), 6 CANL, 7 CANH, **8 S** | Matches | ✅ |
| VCC = 4.5–5.5 V | 5 V rail | ✅ |
| VIO = 2.8–5.5 V | 3.3 V | ✅ |
| Pin 8 S = Silent mode control input | Tied to GND for normal mode | ✅ |

The `/3` suffix genuinely is the VIO variant, so the level-shift-free interface to the G474 works as
claimed. No changes needed.

---

## 4. WS2812B-2020 — UNVERIFIED, blocked

The World Semi datasheet PDF is **image-only** (scanned/rendered artwork, no text layer), so the
parameters could not be extracted. Unconfirmed claims currently in the docs: 36 mA/LED worst case,
VIH = 0.7·VDD, −40…+85 °C rating, and the timing that the DMA driver will depend on.

**This is already the right shape of risk** — assumption **A3** exists precisely to measure the
current rather than trust it. Resolve by: OCR'ing the datasheet, obtaining a text version from
World Semi, or measuring on the bench. The 74AHCT1G125 level-shift decision does not depend on the
exact VIH number: with a 5 V rail the threshold is high enough that 3.3 V logic is marginal under any
plausible reading, so the buffer stays regardless.

---

## 5. Input protection vs. buck ratings — **VERIFIED, and the protection does not work** (CRITICAL)

This was flagged in the original docs as "verify with a scope" and then shipped anyway with a part
chosen from memory. Verified now, and **the input protection on both boards fails to protect
anything.** This defect destroys hardware, where the BOOT0 defect merely prevented booting.

### The numbers (all read from datasheets)

| Device | Rating | Source |
|---|---|---|
| **AP63200/01/03/05** (wheel 5 V buck, dash 3.3 V buck) | **VIN abs max −0.3 to +35.0 V DC**, +40.0 V for 400 ms. Operating 3.8–32 V | Diodes DS41326 Rev 3, Absolute Maximum Ratings |
| LMR33630 (dash 5 V buck) | Operating to 36 V, abs max 42 V | TI datasheet (operating range confirmed; abs-max value still from secondary source — **confirm in the table**) |
| **SMBJ33A** (specified on both boards) | **VC = 53.3 V @ IPP 11.26 A** | SMBJ series datasheets (Littelfuse / Vishay / Bourns, consistent) |
| SMBJ26A | VC = 42.1 V @ 14.25 A | as above |
| SMBJ24A | VC = 38.9 V @ 15.42 A | as above |

### Defect 5.1 — The TVS clamps far above what the regulators survive

The SMBJ33A lets the rail reach **53.3 V** before it is doing its full job. That is:
- **18.3 V above the AP632xx's 35 V DC absolute maximum** — and still 13.3 V above even its
  relaxed 40 V/400 ms rating. **The wheel's buck is destroyed by the very event the TVS is there to
  survive.**
- **~11 V above the LMR33630's 42 V absolute maximum** on the dash.

A TVS whose clamping voltage exceeds the downstream absolute maximum is decorative. Both boards
currently have decorative input protection.

### Why you cannot fix this by just picking a smaller TVS

There is a genuine squeeze, and it is worth seeing explicitly:
- **Lower bound on standoff:** the TVS must *not* conduct during normal operation or a 24 V
  jump start, so VRWM wants to be ≥ 26 V. Below that the TVS sits in avalanche and cooks.
- **Upper bound on clamping:** VC must stay under the regulator's absolute maximum — 35 V for the
  AP632xx.

There is no SMBJ part that satisfies both: even SMBJ24A (standoff too low for jump start) clamps at
38.9 V, still above the AP632xx's 35 V DC limit. **The constraint is not the TVS. It is that a 35 V
converter has no business sitting directly across a vehicle battery.**

### Correction: raise the converter rating, then the TVS fits easily

**Recommended architecture — one 60 V-class converter per board as the single point of
high-voltage exposure, everything downstream living in a benign 5 V world:**

```
12V ─ fuse ─ P-FET ─ SMBJ33A ─→ [60V-rated buck] ─→ 5V ─→ [cheap LV regulator] ─→ 3.3V
                                 ^ only part that ever sees an automotive transient
```

With a 60 V converter, the SMBJ33A's 53.3 V clamp has roughly 7 V of headroom and its 33 V standoff
comfortably clears jump start. The conflict dissolves instead of being squeezed.

**Candidate (NOT yet adopted — needs its own datasheet verification per rule 1a):**
**TI LMR36015** — 4.2–60 V input, tolerant to 66 V, 1.5 A. LCSC `C2863325` (LMR36015AQRNXRQ1),
in stock. HotRod package, so PCBA rather than hand assembly.

- **Wheel** (0.63 A on the 5 V rail): 1.5 A part is comfortable. ✅ candidate fits.
- **Dash**: if the 3.3 V buck is re-fed from the **5 V rail** instead of `+12V_P` — which it must be,
  since the AP63203 cannot face the battery either — then the 5 V buck carries roughly
  0.5 A + (1.4 A × 3.3/5 ÷ 0.9) ≈ **1.5 A**, which is exactly at the LMR36015 limit and therefore
  **too marginal**. The dash needs a larger 60 V part (LMR36520-class ~2 A, or TPS54360-class 3.5 A).
  **Selection open — verify against the datasheet before adopting.**

**Also required by this change:** the AP63203 keeps its job on the dash but is re-sourced from `+5V`,
not `+12V_P`. Its 35 V rating is perfectly fine there; it was only ever wrong at the battery.

**Status:** the architectural fix is settled; the specific 60 V parts are **candidates pending
datasheet verification**, and the docs must not present them as decisions until that is done.

### Verified good news
`AP63200/01/03/05` operating range 3.8–32 V and the 2 A rating are as claimed, so the part is a sound
choice *downstream* of the front-end converter. The original "1.5 A" figure quoted for AP63205 was
wrong in the docs — the family is rated **2 A**.

## 5A. BAT54S in the DAQ front end — **VERIFIED, wrong part** (defect 5.2)

The 8-channel analog front end clamps each ADC input with a **BAT54S**. That is the wrong device
class for a 12-bit input, and the error it injects is large.

| Device | Reverse leakage | Source |
|---|---|---|
| **BAT54S** (Schottky) | **2 µA @ 25 V, 25 °C**; **~100 µA @ 30 V, 100 °C** | BAT54 family datasheets (ST / Vishay / Diodes, consistent) |
| **BAV199** (low-leakage silicon) | **3 pA typ**; 3 nA @ 150 °C | Nexperia BAV199 |

**Why it matters here.** The AFE presents a **5 kΩ Thévenin source** at the ADC node (10 kΩ series ∥
10 kΩ to ground). Leakage current flowing into that impedance is indistinguishable from signal:

- At 25 °C: 2 µA × 5 kΩ = **10 mV** of offset — about **12 LSB** at 12 bits over 3.3 V.
- At 100 °C — entirely reachable for a dash in direct sun, which is the environment we *chose* the
  display for — 100 µA × 5 kΩ = **500 mV**. On a 0–2.5 V signal that is a **20 % error**.

A DAQ channel that drifts half a volt when the car gets hot is worse than no DAQ channel, because it
looks like data.

**Correction:** replace BAT54S with **BAV199** (same SOT-23 dual, ~667,000× lower leakage). Its higher
forward drop is irrelevant here — the clamp is a fault-path device, not a signal-path one.

**Correction (defect 1.7):** an earlier version of this section said the 10 kΩ resistor "does the
real protection work" and called the clamp belt-and-braces. **That was wrong.** The datasheet gives
`IINJ(PIN)` as **−5/0 mA** — negative injection only — and states positive injection "is not possible
on these I/Os"; the governing limit is the **4.0 V absolute-maximum input voltage** (Table 14). So the
clamp is the *primary* protection against a positive fault, and the series resistor's job is to keep
the current low enough that the clamp holds the pin under 4.0 V. That makes low leakage **and** low
forward drop both matter — see defect 1.7 for the resulting open design question.

---

## 5B. Riverdi RVT50HQBNWN00 — **VERIFIED, power architecture was wrong** (defect 5.3, and it's good news)

Source: Riverdi **DS_RVT50HQBNWN00 Rev 1.7**, read directly.

The docs stated: *"3.3 V supply, integrated backlight driver (≈1.2 A at full brightness — drives the
3.3 V buck sizing)."* **All three claims were wrong.**

### What the datasheet actually says

| Parameter | Value |
|---|---|
| Module logic supply **VDD** | 3.0 / **3.3** / 3.6 V — `IVDD` **98 mA typ, 384 mA max** (max is with audio at full volume; we fit no speaker) |
| Backlight supply **BLVDD** | **A completely separate rail**, 3.1 / **5.0 typ** / 5.5 V |
| Backlight current **@ 5.0 V** | **353 mA** at 100 % brightness, 164 mA at 50 % |
| Backlight current @ 3.3 V | 657 mA at 100 % — *the driver is constant-current, so a lower rail costs more current* |
| Interface | 20-pin, 0.5 mm pitch FFC ("RiBUS"), matched cable `FFC0520150` — **pin count claim was correct** |
| Logic levels | VIH 2.0 V min, VIL 0.8 V max — 3.3 V logic drives it directly ✅ |
| INT (pin 7), RST/PD (pin 8) | Both **internally pulled up 47 kΩ**, active low |

Verified RiBUS pinout: 1 VDD, 2 GND, 3 SPI_SCLK, 4 MISO/IO1, 5 MOSI/IO0, 6 CS, 7 INT, 8 RST/PD,
9 GPIO0, 10 DISP_AUDIO, 11 GPIO1/IO2, 12 GPIO2/IO3, 13–16 NC, **17–18 BLVDD**, **19–20 BLGND**.

### Consequences — the dash gets simpler and cheaper

1. **Feed BLVDD from the 5 V rail, not 3.3 V.** At 5 V the backlight draws 353 mA; at 3.3 V it would
   draw 657 mA for the same light. Running it at 5 V is both correct per the datasheet and lower current.
2. **The 3.3 V buck was sized for a load that was never on it.** Real 3.3 V load is MCU (~100 mA) +
   module VDD (~98 mA typ, 384 mA worst case) ≈ **0.5 A**, not 1.4 A. The AP63203 (2 A) is now
   heavily oversized — fine, but no longer load-bearing.
3. **The 5 V buck requirement drops, and this resolves the open selection from §5.** Revised 5 V rail
   load: backlight 353 mA + sensor excitation 200 mA + servo buffers + CAN ≈ 0.6 A direct, plus the
   3.3 V rail reflected (0.5 A × 3.3/5 ÷ 0.9 ≈ 0.37 A) = **≈ 1.0 A**.
   **The 1.5 A LMR36015 candidate now fits the dash as well as the wheel** — one 60 V converter part
   across both boards, instead of needing a larger part for the dash. (Still a candidate until its
   own datasheet is verified.)
4. **Revised dash 12 V input draw:** ≈ (1.65 W + 3.0 W) ÷ 0.85 ÷ 12 V ≈ **0.46 A** — the 2 A input
   polyfuse remains correct with generous margin.

Also correct the earlier assumption **A5** ("Riverdi 3.3 V backlight inrush") — it was aimed at the
wrong rail. The inrush to scope is on **BLVDD/5 V**, not 3.3 V.

---

## 5C. AMS1117-3.3 — **VERIFIED, stability defect** (defect 5.4)

The AMS1117 datasheet specifies a **22 µF tantalum** output capacitor. That is not a packaging
preference — the regulator's compensation **depends on the capacitor's ESR**. The BOM specifies
**22 µF X7R ceramic**, whose ESR is a few milliohms. An all-ceramic output on an AMS1117 can push the
loop unstable and oscillate, putting ripple on the 3.3 V rail that feeds the MCU and (on the wheel)
the display.

This is the classic AMS1117 trap: it usually *appears* to work, which is exactly why it survives
review and then bites one board in ten, or one board when hot.

**Three valid fixes, in order of preference:**
1. **Replace with a ceramic-stable LDO.** The 3.3 V load is only ~100 mA, and modern SOT-23-5 LDOs
   (AP2112K-3.3 class) are explicitly specified for ceramic output caps, with lower quiescent current
   and a smaller footprint. **Recommended** — it removes the failure mode rather than compensating it.
2. Keep the AMS1117 and use an actual **22 µF tantalum** output cap, as the datasheet says.
3. Keep ceramic and add **0.5–1 Ω in series** with it to synthesise the ESR the loop expects.

The original justification for the AMS1117 was "JLCPCB basic part, always stocked." That is still
true and still worth something — but a basic part with a documented stability constraint is only a
bargain if the constraint is honoured. **Selection open; do not capture the schematic with a ceramic
output cap on an AMS1117.**

---

## 5D. DMP3056L — VERIFIED, adequate but thin margin (no defect, one recommendation)

| Datasheet | Value | Our worst case | Verdict |
|---|---|---|---|
| V_DSS | **−30 V** | Reverse-connected supply: FET off, sees the input voltage. −12 V normal, **−24 V reverse jump start** | ⚠ only ~6 V margin |
| V_GSS | ±20 V | Gate clamped by the 12 V zener → ≤12 V | ✅ comfortable |
| I_D continuous | −4.3 A @ V_GS −10 V | 0.29 A (wheel) / 0.46 A (dash) | ✅ ~10× margin |

The part works, but a reverse jump start leaves only 6 V of V_DS margin against a 30 V rating, and
rigor rule 5 asks for derating on exactly this kind of parameter. **Recommendation: move to a −40 V
or −60 V logic-level P-FET.** Same package, same circuit, negligible cost — and reverse protection
that is thin precisely in the scenario it exists for is not worth keeping.

---

## 5E. Encoder input conditioning — checked by analysis (no defect)

The conditioning cell (1 kΩ series, 10 kΩ pull-up, 100 nF to GND) is **asymmetric**, which is worth
understanding rather than discovering later:
- **Falling edge** (contact closes, node pulled to GND through 1 kΩ): τ ≈ 1 kΩ × 100 nF = **100 µs**
- **Rising edge** (contact opens, node pulled up through 10 kΩ): τ ≈ 10 kΩ × 100 nF = **1 ms**

Against the fastest realistic input: a 20-detent encoder spun at ~3 rev/s gives 60 detents/s, i.e.
~8 ms between edges. A 1 ms rise fits with ~8× margin, so the filter does not limit usable encoder
speed. **Enable the STM32 timer input filter (`ICxF`) as well** — the RC handles contact bounce
energy, the digital filter rejects what survives it. Confirm the actual bounce duration on the bench
during HMI bring-up (step 6) rather than trusting the analysis.

---

## 6. Dash pin map — VERIFIED against the AF table (fares much better than the wheel)

Re-checked every dash assignment against the same STM32G474 Table 13 used in §1:

| Assignment | Verdict |
|---|---|
| `EVE_CS/SCK/MISO/MOSI` = PA4/PA5/PA6/PA7 | ✅ SPI1_NSS / SCK / MISO / MOSI — a genuine, complete SPI1 set |
| `SERVO1/2_PWM` = PB6/PB7 | ✅ TIM4_CH1 / TIM4_CH2 — valid pair on one timer |
| `LED_DATA` = PA8 | ✅ TIM1_CH1, and no encoder competes for TIM1 on this board |
| `V12_SENSE`/`V5_SENSE` = PB0/PB1 | ✅ ADC1_IN12 / ADC2_IN12 |
| CAN / USB / SWD / UART | ✅ same as wheel |
| **BOOT0** | ❌ **inherits defect 1.3** — `PB8-BOOT0` is `FDCAN1_RX`. No strap; use the `nBOOT0` option bit |
| `AIN1–8` = PA0–PA3, PC0–PC3 | ✅ **Closed.** All eight are reachable by **ADC1 and/or ADC2** (PA0–PA3 on ADC1/ADC2, PC0–PC3 all `ADC12_*`), so a single ADC1 scan sequence — or ADC1+ADC2 in dual mode — covers the set. No channel is stranded on ADC3/4/5. Exact channel numbers are a CubeMX detail, not a schematic one |

**The dash map survived because it never attempted six hardware encoder pairs** — it asked less of
the alternate-function map, so there was less to get wrong. Only the BOOT0 issue and the open ADC
instance question remain.

---

## 6A. Closed selections — final parts, all verified

| Role | **Final part** | Verified facts | Fit |
|---|---|---|---|
| 12 V→5 V buck (**both boards**) | **TI LMR36015** (VQFN-HR-12 "RNX") | **VIN abs max 66 V**; recommended 4.2–60 V; IOUT 0–1.5 A | SMBJ33A clamps at 53.3 V → **12.7 V margin** ✅ Resolves §5. Wheel 0.63 A, dash ~1.0 A, both inside 1.5 A ✅ |
| 5 V→3.3 V, **WHEEL ONLY** | **AP2112K-3.3TRG1** (SOT-25), LCSC `C51118`, ~$0.083 | 600 mA min, dropout 0.25 V @ 600 mA, **specified for 1 µF X7R/X5R ceramic in and out** | Replaces the AMS1117 and its tantalum requirement (§5C). Wheel 3.3 V load is ~100 mA → 0.17 W ✅ |
| 5 V→3.3 V, **DASH ONLY** | **AP63203WU-7** buck (TSOT-26), fed from `+5V` | 2 A, 3.8–32 V in, 35 V abs max | ⚠ **The dash must NOT use the LDO.** Its 3.3 V load is ~0.5 A, which in an LDO would dissipate 0.5 × 1.7 = **0.85 W in a SOT-25** — far too much. A buck keeps it under 0.2 W. An earlier revision of this table said "LDO (both)", which was wrong; the dash keeps the AP63203 (now sourced from `+5V`, not `+12V_P`) |
| Reverse-polarity P-FET | **DMP3056L retained** — see analysis below | V_DSS −30 V, V_GSS ±20 V, I_D −4.3 A | ✅ for realistic FSAE reverse scenarios |
| CAN bus TVS | **PESD2CAN** (SOT-23) | 24 V standoff, V_BR 26.2–30.3 V, **V_CL 41 V max @ 5 A**, 25 pF | TJA1051 bus pins tolerate ±58 V → **17 V margin** ✅. 24 V standoff clears CAN's −2…+7 V common mode ✅ |
| USB ESD | **USBLC6-2SC6** (SOT-23-6), LCSC `C7519` | IEC 61000-4-2 level 4, 15 kV air / 8 kV contact, very low capacitance | ✅ suits USB 2.0 FS |

### LMR36015 reference values (Table 10-1, 1 MHz variant, 5 V out)
`L = 10 µH` · `COUT = 3 × 15 µF` (2 × 15 µF minimum rated) · `CIN = 4.7 µF + 2 × 220 nF` ·
`RFBT = 100 kΩ` · `RFBB = 24.9 kΩ` · `CFF = 20 pF` · `CBOOT = 100 nF` · `CVCC = 1 µF`.
Pinout: 1,11 PGND · 2,10 VIN · 3 NC (tie to SW) · 4 BOOT · 5 VCC · 6 AGND · 7 FB · 8 PG · 9 EN · 12 SW.
⚠ **LCSC stocks the 400 kHz `LMR36015AQRNXRQ1`**, not a 1 MHz part; order `LMR36015FBRNXR` deliberately. Prefer the **FPWM variant** for constant-frequency EMI next to
the analog front end. Confirm fSW of the exact ordered variant against Table 10-1 before capture.

### DMP3056L — reversing the earlier recommendation, with the reasoning
Earlier I suggested moving to a −40/−60 V P-FET. **Retracted after working the actual stress case:**

- During a **load dump the FET is fully ON**, so V_DS ≈ 0. The 53 V clamp never appears across it.
- **V_GS is the parameter at risk**, not V_DS: with gate pulled to ground through R1, V_GS = −V_IN,
  which a load dump would drive to −53 V against a ±20 V rating. **That is exactly what the 12 V
  zener is for**, and it holds V_GS at −12 V. Zener current at clamp: (53 − 12)/10 kΩ = **4.1 mA**,
  trivial for a 500 mW part.
- **V_DS only matters when reverse-connected** (FET off). Realistic FSAE reverse is a backwards 12–14 V
  battery or bench supply → **14 V against 30 V = 2× margin**, which satisfies rigor rule 5.
- The −24 V reverse-jump-start case would leave only 6 V, but an FSAE car is not jump-started from a
  24 V truck. **Stated as an explicit design assumption rather than engineered against.**

No readily LCSC-stocked −40/−60 V logic-level SOT-23 P-FET was found, so mandating one would have
made the design less buildable for a scenario that does not occur. **DMP3056L stands.**

## 6B. Wheel display changed to colour — **JDI LPM013M126A** (VERIFIED)

Source: **Japan Display Inc. LPM013M126A specification Ver.01**, read directly.

Requirement: colour, while keeping sunlight performance. The answer is a **colour memory-in-pixel
(MIP) reflective LCD** — same physics as the mono Sharp (ambient light *helps*), but with colour.

### The pinout is identical to the Sharp part

| Pin | JDI LPM013M126A | Sharp LS013B7DH05 |
|---|---|---|
| 1 | SCLK | SCLK |
| 2 | SI | SI |
| 3 | SCS | SCS |
| 4 | EXTCOMIN | EXTCOMIN |
| 5 | DISP | DISP |
| 6 | VDDA | VDDA |
| 7 | VDD | VDD |
| 8 | EXTMODE | EXTMODE |
| 9 | VSS | VSS |
| 10 | VSSA | VSSA |

**Pin-for-pin, signal-for-signal identical, both 10-pin FPC.** The display swap is a BOM change —
the schematic sheet, footprint, net names and firmware structure are unchanged. `EXTMODE = H`
enables the hardware EXTCOMIN path exactly as before ("connect to VDD" per JDI note *1-2).

### Verified specifications

| Parameter | Value |
|---|---|
| Resolution / colours | **176 × 176**, **8 colours** (3-bit, one bit each R/G/B) |
| Size | 1.28" (Sharp was 1.26" at 144 × 168 mono — **more pixels and colour**) |
| VDD | 2.7 / 3.0 / **3.3 V max**, **absolute max 3.6 V** |
| VDDA | 2.7 / 3.0 / **≤ VDD** — analog rail must never exceed logic rail |
| V_IH | **VDD − 0.1 V minimum** |
| Power | **115.5 µW max** (data update), 105 µW idle |
| SCLK | 1.00 MHz typ, **2.00 MHz max** |
| Operating temp | **−20 … +70 °C** |
| Reflective contrast | 30:1, viewing angle 65° all directions |

### Three consequences for the design

1. **V_IH = VDD − 0.1 V is tight.** With VDD = 3.3 V the display needs ≥3.2 V for a logic high.
   **Run the display's VDD from the same `+3V3` rail as the MCU** so the MCU's V_OH tracks the
   display's V_IH — at the light loading of a 2 MHz SPI line an STM32 output sits within ~0.1 V of
   its rail, so this works, but do not power the display from a different or lower 3.3 V source.
2. **VDDA must not exceed VDD.** Tie both to `+3V3`. Do not "improve" this by giving VDDA a
   separately filtered higher rail.
3. ⚠ **Operating temperature is −20 … +70 °C** — narrower than the rest of the wheel BOM. A black
   steering wheel in direct sun can exceed 70 °C at the panel surface. **New assumption A9:** measure
   panel surface temperature during a summer track session before trusting this part long-term.

### Sourcing risk (be honest about this)
The LPM013M126A is stocked by **specialty display distributors** (Switch-Science, Youritech,
Data Modul, LCDs-Screen) rather than Digi-Key/Mouser/LCSC. That is a thinner supply line than the
Sharp part, which is JLC-assemblable at `C17500193`. Because the two are pin-compatible, **keep the
Sharp LS013B7DH05 documented as the drop-in mono fallback** — if colour stock fails, fit the mono
part with no board change at all.

## 6C. Interface-by-interface connection validation

Every place two components talk to each other, checked against both datasheets:

| Interface | Requirement | What we provide | Verdict |
|---|---|---|---|
| MCU → TJA1051 TXD | V_IH referenced to **VIO** (3.3 V) | STM32 3.3 V push-pull | ✅ |
| TJA1051 RXD → MCU | Output referenced to VIO = 3.3 V | 3.3 V MCU input | ✅ — and this is *why* the `/3` VIO variant was chosen |
| TJA1051 VCC | 4.5–5.5 V | 5.0 V rail | ✅ |
| TJA1051 CANH/CANL | Bus fault tolerance ±58 V | PESD2CAN clamps at **41 V @ 5 A** | ✅ 17 V margin |
| MCU → 74AHCT1G125 | AHCT TTL input **V_IH 2.0 V** | STM32 3.3 V | ✅ legal, not marginal |
| 74AHCT1G125 → WS2812 DIN | WS2812 needs ≈0.7 × 5 V = **3.5 V** | AHCT output swings to ~5 V | ✅ — and 3.3 V direct would **not** have met it |
| MCU → 74AHCT2G125 → servo | Servo expects ~5 V pulse | Same AHCT translation | ✅ |
| MCU → display SPI | **V_IH = VDD − 0.1 V** | Same `+3V3` rail as MCU | ✅ *provided the rails are common* (see §6B.1) |
| MCU SPI clock rate | Display max **2.00 MHz** | Firmware must cap SPI1 ≤2 MHz | ⚠ **firmware constraint — the STM32 will happily run 20 MHz** |
| LMR36015 EN | *"Can be connected directly to VIN; Do not float."* (Pin Functions table). Abs max EN-to-AGND 66.3 V | Tied directly to `+12V_P` | ✅ the datasheet's own sanctioned arrangement. **An earlier row here invented a "must not exceed VIN by >0.3 V" rule that does not exist — defect 8.11** |
| LMR36015 VIN | Abs max 66 V | SMBJ33A clamps 53.3 V | ✅ 12.7 V margin |
| AP2112K VIN | From 5 V rail | LMR36015 output | ✅ |
| ADC inputs | 0–3.3 V | Divider gives 0–2.5 V from 0–5 V sensors | ✅ 0.8 V headroom |
| Encoder/button lines | 3.3 V logic | 10 kΩ pull-up to `+3V3`, switch to GND | ✅ |
| Paddle sense | ECU-driven line, 5 V or 12 V pull-up | ~~100 kΩ + 1 nF, pins are FT~~ | ❌ **DEFECT 1.8** — pins are `TT_a` (4.0 V abs max), and with no lower resistor the pin sees the full line voltage. Fixed: 150 kΩ/39 kΩ divider, read as ADC |

**One new firmware-facing constraint fell out of this:** the display's 2 MHz SCLK ceiling. It is not
a schematic item, so it would have been easy to miss — recorded here and in the schematic file.

## 6D. Defect 5.5 — P-FET orientation was backwards in the schematic file (CRITICAL)

Found by an independent technical audit of the plain-English guides, which traced a contradiction
back into the source documents.

`wheel-schematic-complete.md` §2.1 specified **Source → `NET_FUSED`; Drain → `+12V_P`** — i.e. source
to the battery. That is **backwards**, and the other three source documents were right all along
(`hardware-selections.md` §3.2 "drain-to-battery orientation"; both Altium instruction files
"source→board").

**The physics.** A P-channel MOSFET's body diode conducts **drain → source**. Correct orientation
(drain to battery) forward-biases that diode in normal operation and reverse-biases it on a reversed
supply, while V_GS collapses to ~0 V so the channel is off too — both paths blocked. Reversed
orientation (source to battery) **still works perfectly with correct polarity**, because the gate
pull-down turns the channel on regardless — but on a reversed supply the body diode becomes
forward-biased and conducts the fault straight into the board.

**Why this one is nasty:** it cannot be caught by testing. The board behaves identically either way
until the day someone connects a battery backwards — the single event the component exists to
survive. It is invisible on the schematic unless you know which way a P-FET's body diode points.

**Corrected** in `wheel-schematic-complete.md` §2.1, with the full reasoning written in place so it
cannot be "tidied" back. **Add to gate G1: physically confirm Q1's source and drain against the
DMP3056L pinout, on the drawn schematic.**

## 6E. Step 1 closures (2026-07)

### LMR36015 variant — CLOSED, **and the earlier note had FPWM backwards**
TI Device Comparison Table (SNVSB49D):

| Orderable | FPWM | fSW |
|---|---|---|
| LMR36015**A**RNXT/R | No | **400 kHz** |
| LMR36015**FB**RNXT/R | **Yes** | 1 MHz |
| LMR36015**B**RNXT/R | No | 1 MHz |

The earlier text said *"prefer the non-PFM (FPWM) variant"* and pointed at the **B** part. That is
self-contradictory: **B has FPWM = No**. Forced-PWM — constant switching frequency at all loads, which
is what you want next to an analog front end and LED drivers — is the **FB** variant.

- **Recommended: `LMR36015FBRNXR`** (FPWM yes, 1 MHz) → the values already in the schematic are
  correct: **L1 = 10 µH, C_OUT = 3 × 15 µF**.
- ⚠ **The part LCSC stocks is `LMR36015AQRNXRQ1`** — the automotive Q1 flavour of the **A** variant,
  which is **400 kHz and FPWM = No**. It is AEC-Q100 qualified, which is a genuine plus for a car, but
  if you buy it **the passives change to L1 = 15 µH, C_OUT = 3 × 22 µF** (Table 10-1, 400 kHz row) and
  the converter will pulse-skip at light load. Decide deliberately; do not let stock availability pick
  silently.

### DMP3056L — CLOSED, adequate
V_DSS −30 V · **V_GS(th) = −2.1 V** · R_DS(on) = 0.035 Ω at V_GS = −10 V · I_D = −4.3 A.
In this circuit the zener holds V_GS at −12 V, far past the −2.1 V threshold, so the FET is fully
enhanced. Conduction loss at 0.29 A is 0.29² × 0.035 ≈ **3 mW**, a **10 mV** drop — confirming the
"~20 mV versus 500 mV for a diode" claim that justified the P-FET in the first place. Even during
crank at ~6 V the gate drive is −6 V, still well past threshold. **Genuinely logic-level here.**

### SMAJ TVS parts — CLOSED
`SMAJ24CA`: 24 V standoff, V_BR 26.7–29.5 V, **V_CL 38.9 V @ 10.3 A**.
`SMAJ5.0A`: 5 V standoff, V_BR 6.40–7.00 V, **V_CL 9.2 V @ 43.5 A**.
Both satisfy rigor rule 5 in their roles (standoff clears the working voltage; clamp is below what
sits downstream). The SMAJ24CA on the paddle lines is what led to defect 1.8.

### Still genuinely open after Step 1

| Item | Why it is still open | Impact |
|---|---|---|
| **RM0440 ADC sampling-time table** | ST's server refused the ~2000-page download twice; no usable mirror found | `AIN_SAMPLE_CYCLES` stays marked UNVERIFIED in firmware. Affects DAQ *accuracy*, not safety |
| HSE crystal exact part | Spec is settled (8 or 16 MHz, ≤50 ppm, CL 8–12 pF, ESR ≤80 Ω, 3225). Choosing a stocked part and computing `C = 2 × (CL − C_stray)` is a procurement step | Load-cap values |
| JDI FPC contact side | Mechanical drawing not obtained | **Footprint** — a top-contact part mirrors the pinout |
| PEC09 / PEC11H / KSC4 mechanicals + bounce | PEC09 datasheet returns HTTP 403; needs another source | Footprint and debounce tuning |
| WS2812B-2020 electrical | Datasheet is image-only, no text layer | A3 stays a bench measurement; the 0.45 A firmware cap holds regardless |

## 7. Everything else — UNVERIFIED

Not yet read, and every parameter quoted for them in the other memory files should be treated as
provisional:

- **Regulators** (AP63205WU-7, AP63203WU-7, LMR33630ADDAR, AMS1117-3.3): output current, input
  range, absolute maximum, and **all** inductor/capacitor values. The BOMs already say "confirm
  against the exact datasheet at capture" — that instruction is load-bearing, not boilerplate.
- **Protection** (DMP3056L, PESD2CAN, USBLC6-2SC6, BAT54S, BAV99, SMBJ33A, SMBJ5.0A, SMAJ24CA,
  SMAJ5.0A): standoff and clamping voltages. **The open question from the start — whether the
  SMBJ33A's clamping voltage stays under the LMR33630's 42 V absolute maximum — is still open**, and
  it is a real one, because a TVS that clamps above the buck's abs-max protects nothing.
- **HMI** (PEC09, PEC11H, KSC4): only distributor parametric data was read. Contact bounce duration,
  switch current ratings, and the detent/shaft mechanical drawings are unconfirmed. The PEC09
  datasheet returned HTTP 403 and needs another source.
- **Riverdi RVT50HQBNWN00**: the 20-pin FPC pinout and the **1.2 A backlight current** are both
  unverified — and that current figure is what sizes the dash 3.3 V buck.

---

## 8. What this episode changed

Three of the four defects found so far were in the *same* document (the wheel pin map), and all
three came from writing plausible-looking detail from memory instead of reading a table. The pin map
was additionally labelled "single source of truth" and "freeze it", which would have propagated the
errors into firmware, the harness drawing, and the board order simultaneously.

New standing rule (added to `engineering-rigor.md` as rule 1a): **no pin number, pin name, package
pin count, or component value enters a document until it has been read out of the datasheet.** Where
a value is genuinely a placeholder, it must say so in the text — "spec, select at capture" — rather
than appearing as a decision.

---

## 9. Step 2 — second full design pass, **twelve further defects**

> The Step 2 pass deliberately used *different activities* rather than re-reading: a scripted
> cross-document sweep, a signal-chain trace asking "what does the other end of this net do at
> power-on?", and a peripheral-instance audit asking for the *channel number* rather than "is this
> pin an ADC pin?". Every defect below was passed by earlier prose reviews.

### Defect 8.1 — the authoritative pin table still held the pre-1.5 assignments (MAJOR)

`wheel-schematic-complete.md` **§9** — the table `board_config.h` names as authoritative — still read
`PB14 / PB15 | ENC5_A / ENC5_B | TIM15_CH1 / TIM15_CH2` and `PC2 / PC3 | LCD_DISP / LCD_EXTCOMIN`,
eight lines above its own footnote explaining that TIM15 has no quadrature decoder.

**Root cause, and the reason this matters more than the defect itself:** the defect-1.5 fix was
applied by matching the string `| ENC5 A / B | PB14 / PB15 |`, which is the format used in *this*
file's tables. §9 writes the same fact as `` | PB14 / PB15 | `ENC5_A` / `ENC5_B` | `` — different
column order, backticks, underscores. The edit reported success and silently missed the one table
that mattered.

Worse, `board_config.h:9` says *"if this file and the schematic definition ever disagree, the
schematic definition is right and this file is a bug."* Followed literally, that rule instructed the
next engineer to **revert the fix** — restoring TIM15 and the double-booked PC2.

**Fix:** §9 corrected. `scripts/check-consistency.py` written so the check is mechanical, not
attentional — it parses §9 for double-booked pins and for encoder rows naming a timer that cannot
decode quadrature, and cross-checks `board_config.h` against it.

### Defect 8.2 — the debug UART disables encoder 3 (MAJOR)

`PA9`/`PA10` (`DBG_TX`/`DBG_RX`) are also `UCPD1_DBCC1`/`UCPD1_DBCC2`, the USB Type-C **dead-battery**
sense inputs. DS12288 Table 12 note 6:

> "After reset, a pull-down resistor (Rd = 5.1 kΩ from UCPD peripheral) can be activated on PB6, PB4
> (UCPD1_CC1, UCPD1_CC2). The pull-down on PB6 (UCPD1_CC1) is activated by high level on PA9
> (UCPD1_DBCC1). The pull-down on PB4 (UCPD1_CC2) is activated by high level on PA10 (UCPD1_DBCC2).
> This pull-down control … can be disabled by setting bit UCPD1_DBDIS = 1 in the PWR_CR3 register."

An idle UART line sits high, so initialising the debug UART arms Rd on the MCU's own `PB6`:

| Board | PB6 / PB4 net | Idle level with 5.1 kΩ Rd | Result |
|---|---|---|---|
| Wheel | `ENC3_A`, 10 kΩ pull-up | 3.3 × 5.1/15.1 = **1.11 V** | Below the 2.31 V `FT_c` V_IH (Table 54) — **ENC3_A can never read high; TIM4 decode fails** |
| Dash | `EVE_INT`, 47 kΩ internal pull-up | 3.3 × 5.1/52.1 = **0.32 V** | Active-low display interrupt **stuck asserted** |
| Dash | `SERVO1_PWM_3V3`, push-pull output | — | Benign; 0.65 mA of wasted drive |

**Why it hid.** Three layers. (1) Neither board uses UCPD — the wheel's CC pins are terminated with
*discrete* 5.1 kΩ resistors — so "UCPD is not in this design" was true and misleading at once; this is
a default-on peripheral function that fires without being instantiated. (2) The coupling is between
two pins nobody would trace together; the AF table correctly says PB6 is TIM4_CH1 and PA9 is
USART1_TX, and the hazard lives only in a footnote linking them. (3) The debug UART is the one
interface everyone assumes is inert — here it is an actuator that shifts the DC operating point of a
pin two ports away. The symptom is maximally hostile: **encoder 3 fails only while you are watching
it**, working in a production build and breaking when the debug harness is attached to diagnose it.

**Fix:** `PWR->CR3 |= PWR_CR3_UCPD1_DBDIS;` before any GPIO configuration, both boards — provisioning
class, like `nSWBOOT0`. Documented in `wheel-schematic-complete.md` §9.1, `dash-schematic-complete.md`
§8.1, and `board_config.h`. **No hardware fix exists**: every encoder-capable timer pair on LQFP-64 is
allocated, so ENC3 cannot move, and shrinking the pull-up to beat Rd lands on V_IH with zero margin.
Bring-up gate: with a debug adapter attached, confirm ENC3 counts both directions.

### Defect 8.3 — wheel rail-monitor ADC channels off by one (MAJOR)

`board_config.h` had `V12_SENSE_CH = 3` and `V5_SENSE_CH = 4`. Verified in DS12288 Table 12:

| Pin | Channel | Pin | Channel |
|---|---|---|---|
| PA0 | ADC12_IN1 | PC0 | ADC12_IN6 |
| **PA1** | **ADC12_IN2** | PC1 | ADC12_IN7 |
| **PA2** | **ADC1_IN3** | PC2 | ADC12_IN8 |
| PA3 | ADC1_IN4 | PC3 | ADC12_IN9 |

`PA1` is IN**2**, not IN3. So channel 3 sampled `PA2` (the 5 V divider) as the 12 V monitor, and
channel 4 sampled `PA3` — which on the wheel is **`BTN3`**. Both failures are quiet:

- 12 V monitor reads 2.5 × 5.7 = **14.25 V** — a perfectly plausible charging voltage, so it never
  looks wrong.
- 5 V monitor reads 6.6 V idle and **0 V whenever a driver presses button 3** — a phantom rail
  collapse correlated with a button.

**Why it hid.** §1.4b recorded "ADC instance availability per pin" as checked, and it *was*: the
question asked was "can ADC1 reach PA1?", which is true. The channel *index* was never the question.
The numbering is off-by-one-trappy in both directions — PA0 = IN1, so counting from either PA0 = IN0
or PA0 = IN1 produces a self-consistent-looking answer.

**Fix:** channels corrected to 2 and 3. The verified table above is now written into `board_config.h`
for both boards, including `AIN_CHANNELS` for the eight dash DAQ pins, so Step 3 cannot re-derive them
from pin numbers.

### Defect 8.4 — capture instructions still specified rejected parts (MODERATE — see the severity note)

> **Severity, honestly stated.** The sheet-by-sheet sections of *both* Altium instruction files sit
> under a `⚠ SUPERSEDED BY THE SCHEMATIC-DEFINITION FILE ⚠` banner that already warns they "still name
> parts that have since been rejected". So this is a second line of defence failing, not the first —
> lower severity than it first appeared, and it is recorded that way rather than inflated. It is still
> worth fixing: a banner is weaker than correct text, and readers skim. **`PROJECT-LOG.md` gate G2 and
> the dash BOM carried no banner at all.**

`dash-pcb-altium-instructions.md` §1.1 still said:

1. *"**LMR33630ADDAR** buck → +5V @ 2 A, 400 kHz … 2×22 µF in, 2×47 µF out"*. The LMR33630 was
   **rejected** in §5: its 42 V absolute maximum is 11 V *below* the SMBJ33A's 53.3 V clamp. The BOM
   and `dash-schematic-complete.md` had both moved to the LMR36015; this file had not.
2. *"**AP63203WU-7** buck fed from `+12V_P`"*. The BOM explicitly re-sourced this from `+5V` because
   its 35 V absolute maximum must never face the battery — the *exact* failure mode of defect 5.1.

The **wheel** file had the identical problem: §3.1 step 3 specified the rejected **AP63205WU-7**
(the original 35 V-abs-max part from defect 5.1) and step 4 the rejected **AMS1117-3.3** (the
tantalum-required LDO from defect 5.4); §2 still listed an AP63205 TSOT-26 footprint check and §4 an
"AP63205 hot loop" placement rule. `PROJECT-LOG.md` gate G2 — **not** under any banner — still named
the LMR33630 hot loop.

All corrected, and each now says explicitly which part is *not* to be fitted and why, so the stale
version cannot be followed from memory.

### Defect 8.5 — both BOMs omitted components the buck datasheet calls *required* (MAJOR)

Reversing the BOM (lens 4) rather than reading it forward: the LMR36015's mandatory support parts
were described in prose inside `U1`'s notes cell but **were not orderable line items**.

| Part | Datasheet | Consequence if absent |
|---|---|---|
| `C_BOOT` 100 nF | §10.2.1.2.7 "requires" | High-side FET cannot be driven — **buck does not switch at all** |
| `C_VCC` 1 µF | §10.2.1.2.8 "requires … for proper operation" | Internal LDO unstable |
| `R_FBT` / `R_FBB` | Table 10-1 | **No output setpoint** — FB floating |
| `C_FF` 20 pF | §10.2.1.2.9 | Degraded phase margin |
| 2 × 220 nF at VIN | §10.2.1.2.6 "**must** be used … place two 220-nF ceramic capacitors at each VIN-PGND location" | No HF bypass for the internal control circuits |

The dash was worse: its `C2;C3` (2 × 22 µF in) and `C4;C5` (2 × 47 µF out) were the **LMR33630
400 kHz** values, left behind when only the MPN cell was updated — while
`dash-schematic-complete.md` asserted the passives were "identical" to the wheel's. They were not.

**Fix:** all six parts added as line items to both BOMs; dash input/output capacitors corrected to
the wheel-identical TI Table 10-1 1 MHz values. The two documents now actually agree.

### Defect 8.6 — wheel capture instructions still showed the pre-1.8 paddle tap (MODERATE)

`wheel-pcb-altium-instructions.md` §3.3 step 3 still read *"tap 100 kΩ → `PADDLE_UP_SNS` → 1 nF to
GND at MCU pin"* — a single series resistor with **no lower divider leg**, which is defect 1.8
verbatim. Capturing from it puts the full paddle-line voltage on a 4.0 V pin.

Same severity caveat as 8.4: this sits under the superseded banner, so it is a backup layer failing
rather than the primary instruction. Corrected anyway to the 150 kΩ / 39 kΩ divider plus the 8.7
clamp, with an explicit "do not fit the 100 kΩ single-resistor tap shown in earlier revisions".

### Defect 8.7 — paddle sense pins exceed their absolute maximum during a TVS clamp (MAJOR)

Traced end-to-end (lens 1): `D3`/`D4` are **SMAJ24CA**, clamping at **38.9 V**. Through the
150 kΩ/39 kΩ divider that is 38.9 × 39/189 = **8.03 V** at `PB0`/`PB1` — pins whose absolute maximum
is **4.0 V** (DS12288 Table 14, `TT_xx`). Defect 1.8 fixed the *steady-state* case (16 V charging
system → 3.30 V); the *transient* case was never checked.

There is no self-rescue. Table 15 note 3: *"Positive injection (when V_IN > V_DD) is **not possible**
on these I/Os"* — meaning these pins have **no upper clamp diode to VDD**, so the overvoltage appears
across the pin structure rather than being shunted into the rail. The absence of an injection *number*
here is not permission; it is the statement that the mechanism which would have saved the pin does not
exist.

**Fix:** `D14`/`D15`, **BAV199** from each sense net to `+3V3`. The clamp holds the pin near 3.9 V
while the 150 kΩ upper leg limits diode current to (38.9 − 3.9)/150 kΩ = **0.23 mA**, trivial for a
BAV199. BAV199 rather than a Schottky for the same reason as the DAQ front end — leakage into a
high-impedance node *is* signal error (defect 5.2). This makes the wheel consistent with the dash,
where the equivalent clamp was already declared load-bearing (defect 1.7).

### Defect 8.8 — the dash 3.3 V buck had no bootstrap capacitor (MAJOR)

Found by the power-tree walk (lens 3), which re-derives each rail instead of re-reading it. Walking
`+5V` → `U2` (AP63203) → `+3V3` meant listing what `U2` needs to switch, and the bootstrap capacitor
was not in the BOM.

AP63203 datasheet **Table 2** (recommended components, 3.3 V output) lists four parts: `L` 3.9 µH,
`C1` 10 µF, `C2` 2 × 22 µF, **`C3` 100 nF — the BST-to-SW bootstrap capacitor**. The dash BOM had the
inductor and both capacitor banks and **no C3**. Without it the high-side FET cannot be driven and the
3.3 V rail never comes up — the MCU, the display logic and the whole analog front end are on that rail.

Identical failure mode to defect 8.5 on the other converter, and identical cause: the support parts a
switcher needs are easy to see when you ask "what does this chip require?" and invisible when you read
down a BOM. Added as `C_BST2`.

### 8.8b — AP63203 now VERIFIED, and the open L2 item is closed

The datasheet had never been read (`bom-FSAE-DASH-revB.csv` literally said *"CONFIRM against AP63203
datasheet"*). Read now:

| Parameter | Datasheet | Design | Verdict |
|---|---|---|---|
| VIN absolute maximum | **35 V DC** (40 V for 400 ms) | Fed from the regulated `+5V`, **not** `+12V_P` | ✅ — and this is exactly why the re-source was mandatory. On the raw rail the SMBJ33A's 53.3 V clamp would destroy it, the same mechanism as defect 5.1 |
| Switching frequency | **1.1 MHz** | — | — |
| Inductor range | "approximately 2.2 µH to 10 µH", DCR < 100 mΩ | `L2` = **4.7 µH** | ✅ in range |
| Table 2 tabulated L @ 3.3 V | 3.9 µH | 4.7 µH | ✅ acceptable — see derivation |
| C1 input | 10 µF | 2 × 10 µF | ✅ |
| C2 output | 2 × 22 µF | 2 × 22 µF | ✅ exact |
| Output setpoint | Tables 2/3 list **no R1/R2** — the AP63203 is **fixed 3.3 V** | no external FB divider drawn | ✅ correct, and *not* an omission like 8.5 |

**L2 derivation at our actual operating point** (Eq. 7, `L = V_OUT(V_IN − V_OUT) / (V_IN · ΔI_L · f_SW)`).
The tabulated 3.9 µH assumes a higher input voltage; ours is 5 V:

- For the datasheet's 30–50 % ripple target on a 2 A part (ΔI_L = 0.8 A): L = 3.3 × 1.7 / (5 × 0.8 × 1.1 MHz) = **1.28 µH**
- At the fitted **4.7 µH**: ΔI_L = 3.3 × 1.7 / (5 × 4.7 µH × 1.1 MHz) = **0.217 A**
- I_peak = 0.5 + 0.217/2 = **0.61 A** against an `I_sat ≥ 3 A` specification

So 4.7 µH is comfortably inside the recommended range and gives *less* ripple than the target. That is
deliberate rather than sloppy: this rail runs at 0.5 A, a quarter of the part's capability, and the
datasheet says *"use a larger inductance for improved efficiency under light load conditions."*
**Open item closed — keep 4.7 µH.**

### Defect 8.9 — the dash power budget described a converter the board does not have (MINOR)

`dash-schematic-complete.md` §7 said *"0.37 A reflected from the 3.3 V **LDO**"* and carried a note —
*"the LDO dissipation: 0.5 A × (5 − 3.3) V = 0.85 W in a SOT-25 … the one number on the dash worth
re-checking"* — three lines below a bullet correctly stating the rail is *"supplied by the AP63203
buck, **not** an LDO"*, and while §9 already recorded the concern as **closed**. The same section
asserted and denied the same fact.

No board consequence: the **0.37 A figure is right for a buck** (0.5 A × 3.3/5 / 0.9), and an LDO would
have reflected the full 0.5 A. But a reader chasing a flagged 0.85 W thermal problem would be chasing
a rejected architecture. Rewritten with the derivation shown and the comparison explicitly labelled as
the rejected alternative.

**Re-derived power tree, both boards (lens 3):**

| Rail | Source | Worst-case load | Rating | Margin |
|---|---|---|---|---|
| Dash `+3V3` | AP63203 buck from `+5V` | 0.098 A display logic (0.384 abs max) + 0.10 A MCU/CAN/analog → **0.5 A** worst case | 2 A | 4× |
| Dash `+5V` | LMR36015 from `+12V_P` | 0.353 backlight + 0.20 sensor excitation + 0.01 servo buffers = 0.563 A direct, **+ 0.37 A** reflected = **0.93 A** | 1.5 A | 1.6× |
| Dash `+12V_P` | J1 via polyfuse | 4.65 W / 0.85 / 12 V = **0.46 A** | 2 A hold | 4.3× |
| Wheel `+3V3` | AP2112K LDO from `+5V` | ≈0.10 A → 0.17 W dissipation | 600 mA | 6× |
| Wheel `+5V` | LMR36015 from `+12V_P` | LEDs capped at **450 mA** in firmware + MCU/CAN | 1.5 A | — |

The wheel's 5 V margin is not a passive property — it is **enforced by the single LED current-cap
function**, and since Rev B.5 the ceiling is picked at run time from the measured supply rather than
compiled in: **vehicle 450 mA · USB enumerated 300 mA · USB before enumeration 0 mA** (`power.h`).
That cap is a power-tree component, and any pattern code that bypasses it invalidates this row. This is why rule "all writes through one function" is a rigor rule
and not a style preference.

### Defect 8.10 — every capacitor on the clamped 12 V rail was under-rated (MAJOR)

`D1` is an SMBJ33A: it **clamps `+12V_P` at 53.3 V**. Four BOM lines across the two boards specified
**50 V** capacitors on that rail — `C1`/`C2`/`C5` on the wheel and `C1`/`C2`/`C3` on the dash — so
they were rated *below* the voltage produced by the exact event the TVS exists to handle. One wheel
note even called the 50 V rating *"deliberate for load-dump margin"*, which inverts the actual
relationship.

The LMR36015 datasheet §10.2.1.2.6 asks for input capacitors *"rated for at least the maximum input
voltage that the application requires; **preferably twice** the maximum input voltage"*, and states
outright that *"the 220 nF must also be rated at **100 V** with an X7R dielectric."*

**This is defect 5.1 repeated one component further along.** That defect was the TVS clamping above
the *buck's* absolute maximum; the fix raised the converter to a 66 V part. Nobody then asked the same
question of the passives sitting on the same node. Corrected to 100 V, which also buys back a large
amount of X7R capacitance lost to DC-bias derating at 12 V.

Now enforced by `scripts/check-consistency.py`, which flags any input capacitor on the 12 V entry
stage rated at 50 V.

### Defect 8.11 — a fabricated datasheet citation (MAJOR, in kind rather than consequence)

`wheel-schematic-complete.md` §2.2 and `datasheet-verification.md` §6C both stated that the LMR36015's
*"only constraint is that EN must not exceed VIN by more than 0.3 V"*, and used that to justify tying
EN to VIN.

**No such constraint exists in the datasheet.** The Pin Functions table says only: *"Enable input to
regulator. High = ON, low = OFF. Can be connected directly to VIN; Do not float."* §9.3.2 repeats it.
The 0.3 V appears to be a misreading of the absolute-maximum table, where VIN-to-PGND (66 V) and
EN-to-AGND (66.3 V) are two **independent** limits referenced to ground — the 0.3 V difference between
those two numbers is not a relative rule.

The circuit is correct: connecting EN to VIN is precisely what the datasheet sanctions. **The
conclusion was right and the evidence was invented**, which is the dangerous combination — a
fabricated citation reads exactly like a verified one, survives review because the design it defends
is sound, and quietly licenses the next person to trust the rest of the row. Found by an audit agent
that checked citations against the datasheet text rather than checking the design against the citation.

### Defect 8.12 — the variant warning pointed the wrong way (MAJOR — a buying error)

Three files said *"stocked variants are 1 MHz"* about the LMR36015. **The reverse is true:** LCSC
stocks `LMR36015AQRNXRQ1`, the **400 kHz, non-FPWM** automotive variant, while the part every passive
value in the design assumes is the 1 MHz `LMR36015FBRNXR`. §2.2 of the same wheel file said this
correctly two hundred lines earlier; the open-items table at the end contradicted it.

Acting on the wrong version means buying the 400 kHz part and fitting 10 µH / 3 × 15 µF instead of the
15 µH / 3 × 22 µF it needs. Corrected everywhere, and stated as a *buying hazard that remains live*
rather than a closed question — the decision is closed, the risk of picking up the wrong part from
stock is not.

### Defect 8.13 — a designator cross-reference copied between boards (MINOR, but a build error)

The dash BOM's `C16-C20` note was copied verbatim from the wheel's and told the builder *"VDDA pin 29
is covered by `C8`/`C9`."* True on the wheel. **On the dash, `C8`/`C9` are the AP63203's 2 × 22 µF
output capacitors** — VDDA there is `C10`/`C11`, exactly as `dash-schematic-complete.md` §8.0 line 261
and the dash guide both say.

Consequence if acted on: someone reconciling the BOM against the schematic finds VDDA apparently
double-decoupled and the 3.3 V rail apparently missing its output caps, and "fixes" whichever one they
trust less. The number of parts is right; the pointer is wrong, which is worse than an omission
because it reads as a checked cross-reference.

**Found by a spot-check, not by the consistency script** — the script cross-checks values and pin
tables, not free-text notes inside a BOM cell. Same class as 8.4/8.6: prose in one file describing
another file, which nothing mechanically verifies. Found while verifying the plain-English guides,
because the guide (correctly, from the schematic) said `C10`/`C11` and the BOM said `C8`/`C9` — the
disagreement was only visible because two independently-written sources were read against each other.

### Defect 8.14 — the ABM8's *default* configuration is the wrong part (MAJOR — a buying error)

**ABM8 datasheet rev. 07-29-20 read first-hand** (user-supplied PDF). Everything the design asserts
about the part confirms exactly — and the datasheet also surfaced a hazard nothing in the project had
recorded.

**Confirmed, first-hand, against the real document:**

| Claim in the design | Datasheet | Verdict |
|---|---|---|
| ESR ≤ 70 Ω at 16 MHz | Table 1: 16.000–19.999 MHz (Fund) = **70 Ω** | ✅ |
| ESR 400 / 120 / 50 Ω at 8 / 12 / 20 MHz | Table 1: 8.000–9.999 = **400**; 12.000–15.999 = **120**; 20.000–29.999 = **50** | ✅ all four rows of the frequency-choice table |
| C0 ≤ 3 pF | Shunt capacitance **3.0 pF max** | ✅ and it is a *standard* spec, not an option |
| Drive level ≤ 100 µW | **100 µW max**, 10 µW typ | ✅ (the 10 µW typ supports the note that real drive sits far below the 117 µW worst case) |
| Aging ±2 ppm/yr in the ppm budget | **±2 ppm** first year | ✅ |
| 3.2 × 2.5 mm, pins 2 and 4 = case ground | Outline drawing; **0.80 mm** max height for `ABM8` (not `ABM81`/`ABM82`) | ✅ |

So the frequency-selection table, the CL table, the 4.4× headroom and the ±91 ppm budget all stand
unchanged. **Every gm_crit value re-derives to three decimal places from the datasheet's own ESR
table.** This is the good outcome of reading the source: not a correction, but the difference between
"these numbers are right" and "these numbers are asserted."

**The defect.** The design specified the crystal by its *electrical* requirements and named the series.
It never recorded that on the ABM8, **the two figures the whole analysis depends on are order options
with unsuitable defaults**:

| Parameter | ABM8 **standard** (blank field) | Needed | Option |
|---|---|---|---|
| Load capacitance | **18.0 pF** | 8 pF | `8` |
| Operating temperature | **−10…+60 °C** | −40…+85 °C | `D` |
| Tolerance / stability | ±50 / ±50 ppm | ±30 / ±30 ppm | `4` / `Y` |

**Consequence of ordering "an ABM8, 16 MHz":** CL 18 pF gives
`gm_crit = 4·70·(2π·16 MHz)²·(3 + 18 pF)²` = **1.248 mA/V against the MCU's 1.5 mA/V — 1.2× margin**,
and the load capacitors would need to be 26 pF rather than the 6 pF fitted. That is a board that may
not oscillate at all, and whose failure is temperature-dependent and intermittent when it half-works.
The temperature default is separately disqualifying: −10…+60 °C is not a vehicle rating.

Correct part: **`ABM8-16.000MHZ-8-D4Y-T`**. Format
`ABM8[height]-[freq]MHZ-[CL]-[custom ESR]-[temp][tol][stab]-[packaging]`, blank fields omitted; ESR is
blank because 70 Ω *is* the standard band. The BOM's previous placeholder string had the temperature,
tolerance and stability as three separate dash-delimited fields; they are one three-character field.

Note what is **not** claimed: the tolerance and stability defaults are harmless here — ±50/±50 ppm
gives ±131 ppm total, still 38× CAN's requirement. Only CL and temperature are load-bearing. Flagging
all four equally would train a reader to discount the warning.

**Class:** same as defect 8.12 (the LMR36015 400 kHz variant) — a **live buying hazard** rather than a
design error. The design is right; the risk is that a purchaser reads the series name, finds a stocked
standard part, and fits it. Both boards' BOMs now carry the full option string and the reason.

**How it was found:** the user supplied the datasheet after being told the exact part number was not
verified. Nothing in the project's own process would have caught it — the specification was internally
consistent, the electrical numbers were all correct, and a consistency script cannot know that a
vendor's default option differs from the value the design assumes. **A spec written in engineering
units is not an orderable part**, and the gap between them is invisible until someone opens the
ordering-information page.

## §10 — Rev B.5: collapsing the CAR/SIM variants into one build

The user asked whether USB could be present on both versions of the wheel, so that the only
difference is which cable is plugged in. **It already was** — `J2`, `U5`, the CC pull-downs and the
shield network were fitted on both variants; only `R_VBUS`, the 0 Ω link that lets VBUS *power* the
board, was DNP on car boards. Fitting it everywhere and deleting the variants surfaced three defects.

### Defect 8.15 — USB back-feeds the vehicle rail through the buck (MAJOR — new interaction)

With `R_VBUS` fitted **and** the 12 V front end fitted — a combination neither variant had — there is
an unblocked path from the USB port to the vehicle connector:

```
VBUS 5.0 V → D6 BAT60A (~0.3 V) → +5V ≈ 4.7 V → L1 → SW
           → LMR36015 high-side body diode (SW→VIN, ~0.7 V) → +12V_P ≈ 4.0 V
           → Q1: V_GS = 0 − 4.0 = −4.0 V vs V_GS(th) −2.1 V → channel ON
           → NET_FUSED → F1 → J1 pin 1
```

The buck's high-side switch is a bootstrapped NMOS (it has a `C_BOOT`), so its body diode is oriented
`SW` → `VIN` and conducts whenever the output is driven while `VIN` is dead. `Q1` then turns *on*
because its gate is held at ground by `R1` while its source rises — the reverse-polarity FET
conducting in the direction it was never asked about.

**Severity is in the detail, so it is stated per case rather than as one adjective:**

| Case | Behaviour | Verdict |
|---|---|---|
| USB only, `J1` unmated (the desk case) | ~4 V on an unmated pin | harmless |
| Both connected, car live | buck at 5.0 V, `D6` reverse-biased at 4.7 V | no conflict |
| Both connected, **car off** | USB tries to energise the vehicle 12 V bus; the host port current-limits | the real one |

**The obvious fix is worse than the defect.** A series Schottky on the buck output blocks it and
costs ~0.4 V — but the **TJA1051T/3 minimum supply is 4.75 V**, so 5.0 − 0.4 = 4.6 V puts the CAN
transceiver out of spec on every board, permanently, to guard against an occasional annoyance.
Rejected. The body-diode path cannot be blocked without a series element, so it is **accepted and
documented**, with two pieces of real work attached:

1. **Firmware threshold.** `+12V_P` at ~4 V must never read as "vehicle present" — `power.h` asserts
   at 7 V and releases at 6 V, and `test_power.c` sweeps the entire 3.0–5.5 V back-feed band rather
   than checking one convenient point. Getting this wrong asks a 500 mA host port for a 450 mA LED
   cap on top of 180 mA of other load.
2. **Bring-up gate.** Scope `+12V_P` with USB in and `J1` mated, watching for the buck
   hiccup-oscillating as its back-fed `VIN` crosses UVLO. The LMR36015's UVLO threshold has *not*
   been read, so this is an observation to make, not a prediction to trust.

**Class:** L19's signature shape — an interaction between two correct-looking choices. `R_VBUS` is
fine. The buck is fine. `Q1` is fine. The path only exists when all three are populated together,
which is exactly what deleting the variants did.

### Defect 8.16 — a footprint described as "provided" that never existed (MODERATE)

`sim-variant-instructions.md` §2 read: *"Optionally read CC line voltage (ADC on a divider — DNP
footprint `R_CC_SNS` provided) to detect a 1.5 A/3 A source and lift the cap."*

**`R_CC_SNS` appears in no schematic definition and in neither BOM.** It is not a DNP part that was
forgotten in a fitted-list; it is a designator that exists only in that sentence. Anyone planning the
USB power strategy from that document would have designed around a footprint that would not be on the
board when it arrived.

**Not added — deleted, and that is the finding.** Lifting the LED cap above 300 mA is an optimisation
with no safety content (300 mA is safe on *any* compliant source), and building it needs two facts
this project has not verified: an ADC channel number for a spare pin from DS12288 Table 12, and the
USB-C Rp advertisement currents from the Type-C specification. **Adding a footprint whose function
rests on two unread documents is how placeholders get built into boards** (L35). The honest close is
to remove the promise and record what it would cost to make good on it.

Same family as 8.4/8.6/8.13: prose in one file describing another file, which nothing mechanically
verifies. `check-consistency.py` now fails on any designator that a memory file references but no BOM
contains.

### Defect 8.17 — "clock USB from the crystal" is arithmetically impossible (MODERATE)

`wheel-schematic-complete.md` §3.2 said: *"Since CAN requires a crystal anyway, clock both from the
HSE and delete a whole class of clock-accuracy questions."*

| | Requirement | Implied VCO |
|---|---|---|
| USB FS 48 MHz on PLL"Q" | VCO = 48 × Q, Q ∈ {2,4,6,8} | {96, 192, 288, 384} MHz |
| SYSCLK 170 MHz on PLL"R" | VCO = 170 × R, R ∈ {2,4,6,8} | {340, 680, 1020, 1360} MHz |

**Disjoint**, and everything above 344 MHz breaks the Table 46 VCO ceiling regardless. No PLL
configuration serves both. USB must use **HSI48 + CRS**, trimmed against the host's 1 kHz SOF, which
comfortably meets USB FS's 2500 ppm.

Nothing was damaged by this — no hardware depended on it, and USB had never been brought up. What
makes it worth a number is *why it survived*: it is a tidy, plausible simplification of the kind a
good design really does make, sitting in a paragraph whose surrounding claims are all correct. The
crystal genuinely is mandatory; CAN genuinely does need it; the conclusion drawn from those two true
statements simply does not follow.

**Fixed in the place that can enforce it.** `clock_config.h` now carries the arithmetic and a static
assertion that fires if a future PLL re-tune ever makes 48 MHz reachable on PLLQ — guarding the
*argument*, not just today's numbers. `usb_clock_init()` implements HSI48 + CRS with the reload
derived (`f_target / f_sync − 1 = 47999`) rather than pasted. CRS `FELIM` is left at its reset value
and recorded as an RM0440 refinement, not as a placeholder: nothing about it is wrong, it is simply
not optimal, and RM0440 has never been downloadable in this project.

### Defect 8.18 — five components specified in the schematic and absent from the BOM (MAJOR)

Found by the check written for 8.16, on its first run — not by looking for it. Once the checker
resolved every `X_NAME`-style designator in the memory files against the BOMs, five came back with no
BOM line at all. **Two of them are fitted parts**, and one of those protects the display:

| Ref | What it is | Consequence of the omission |
|---|---|---|
| **`R_EXTMODE`** | 0 Ω strap tying the JDI panel's `EXTMODE` pin to `+3V3` | **Not assembled → `EXTMODE` floats.** §7 rule 3: floating leaves COM inversion undefined, DC bias builds across the liquid crystal and **permanently damages the panel**. The most expensive single part on the wheel, on a specialty-distributor lead time |
| **`C_U6`** | 100 nF decoupling at the `74AHCT1G125` LED buffer | Not assembled → the one IC on the board with no local decoupling, driving a 24-LED chain with fast edges |
| `R_EXTMODE_L` | the unused half of that strap (DNP) | No footprint → the alternative is not actually available |
| `R_PG` | optional power-good pull-up on `U1` pin 8 (DNP) | No footprint → "optional" was never optional |
| `J_SWD` | Tag-Connect TC2030-CTX | Present as a row, but with an **empty designator cell**, so it resolved against nothing |

The three DNP/no-cost lines matter less on their own, but they share the cause: **a BOM built by
listing the parts you buy, checked against a schematic that also specifies parts you place.** A 0 Ω
resistor and a 100 nF capacitor are the two least interesting lines on any BOM, which is exactly why
nobody re-reads them — and `R_EXTMODE` is a 0 Ω resistor whose absence destroys a display.

This is 8.16 generalised. That defect was a designator in a document with no part behind it; this is
a part in a document with no BOM line in front of it. **Same gap, opposite direction**, and the same
check catches both because it asks one question of every reference: *does this designator exist in the
BOM for this board?*

**The checker is now per-board.** Pooling both BOMs would let a wheel-only part excuse its absence
from the dash — defect 8.13's mistake in a different costume — so `wheel-*.md` resolves against the
wheel BOM and `dash-*.md` against the dash BOM.

Two exclusions are deliberate and narrow: a reference the surrounding text calls **internal** (the
STM32's own 200 kΩ `R_F` is a datasheet symbol, not a part), and one being **discussed as retired**,
which is how a phantom gets removed honestly rather than silently.

### The number that only moved because the variants merged

Not a defect — a consequence, recorded because it is the best argument for the change. The USB LED
cap was **350 mA**, correct while the CAN transceiver was DNP on sim boards (110 + 350 = 460 mA
inside a 500 mA allowance). Fitting the transceiver on every board spends 70 mA that figure never
saw: **110 + 70 + 350 = 530 mA, which does not fit.** The cap is now **300 mA**.

Nothing would have flagged it. It was a `#define` chosen by a build flag, correct against a BOM that
changed underneath it. The firmware now measures the supply (`V12_SENSE`, hardware that already
existed) instead of compiling an assumption about it — and the pre-enumeration cap is **zero**,
because a device may draw only 100 mA before the host configures it and the non-LED load nearly
consumes that alone.

### Defect 8.19 — the BAV199's third pin was never specified (MODERATE, and it stops capture dead)

**Found by the user, reading the plain-English guide and asking why a three-pin part only had two pins
described.** That is the right question and nothing in this project would have asked it.

Every document described the clamp *electrically* — "upper diode anode → net, cathode → `+3V3`; lower
diode cathode → net, anode → `GND`" — and **no document anywhere gave a physical pin number**. Worse,
that phrasing reads as though the sense net connects to the part **twice**, which is what you would do
with two independent diodes and is not how this part is built.

**BAV199 data sheet read first-hand** (Nexperia, 1 April 2023, saved to
`hardware/lib/datasheet-BAV199-nexperia.pdf`). §1: *"The diodes are connected in series."* Table 2:

| Pin | Symbol | Description | This design |
|---|---|---|---|
| 1 | `A1` | anode (diode 1) | `GND` |
| 2 | `K2` | cathode (diode 2) | `+3V3` |
| 3 | `K1, A2` | cathode (diode 1) **and** anode (diode 2) | **the sense net** |

The net lands on **one pin**. Pin 3 is simultaneously the upper diode's anode and the lower diode's
cathode, because the two diodes already meet inside the package — which is exactly what lets a
three-terminal part clamp both rails.

**Why this is more than a documentation nit.** SOT-23 dual diodes come in three internal arrangements
that share a package, a silkscreen and a nearly identical schematic symbol: **common cathode**
(BAV70), **common anode** (BAW56) and **series** (BAV99, BAV199). **Only the series arrangement can
clamp both rails from one net** — with a common-cathode part the circuit is not merely wrong, it is
unwireable. A future substitution for availability, checked on package and leakage alone, breaks the
clamp silently. Both BOMs now carry the pin map and an explicit non-interchangeability warning.

⚠ **A cautionary note on how this was nearly got wrong.** The first attempt to answer this used a web
fetch, whose summariser reported the BAV199 as **common cathode** — confidently, and with a note that
it could not parse the compressed PDF. Had that been believed, the "correction" would have destroyed a
working circuit. **Reading the actual pages is what settled it**, exactly as with the ABM8 and the
Molex drawing. A summary of a document is not the document.

### Defect 8.20 — the wheel guide never received the 8.7 correction (MODERATE — a doc regression)

Defect 8.7 established that `D14`/`D15` need **both** halves of the BAV199, because `D3`/`D4` are
**bidirectional** SMAJ24CA parts and a negative transient presents −8.03 V at the pin. That correction
was applied to `wheel-schematic-complete.md` §4.3, to the wheel BOM, and to this file.

**It was never applied to `memory/plain-english/wheel-guide.md`**, which still listed only the upper
clamp — "anode → `PADDLE_UP_SNS`, cathode → `+3V3`" — and whose prose discussed only the positive
fault. Anyone capturing the paddle sheet from the guide would have fitted half a clamp on the one
circuit the schematic file calls *"load-bearing, not belt-and-braces."*

Three files updated, the fourth missed. Identical in shape to defect 8.1 and to the "eleven"/"twenty-four"
count that survived a search-and-replace (L26). **A correction is not applied until it is applied to
every file that describes the thing**, and the plain-English guides are consistently the ones left
behind, because they restate rather than duplicate — so a literal search for the old text does not find
them.

### Also flagged while verifying 8.19 — the *justification* for the lower clamp is imprecise

Not a defect in the circuit; a defect in the reasoning written beside it. §4.3 says the lower half is
needed "against an absolute minimum of VSS − 0.3 V". **A silicon diode clamps at about −0.7 V**, which
does not hold the pin above VSS − 0.3 V either. The lower diode cannot deliver what that sentence
claims for it.

What it *does* deliver is worth stating correctly: DS12288 Table 15 note 3 says positive injection is
not possible on these I/Os, which implies a **negative-side internal structure does exist**, so the pin
would self-clamp near −0.7 V regardless. The external diode's real job is to **carry that current
instead of the MCU's ESD structure**, which is rated for one-off static events rather than for
conducting on every negative transient the car produces. With the 150 kΩ upper leg the current is
(38.9 − 0.7)/150 kΩ ≈ **0.25 mA**, far inside the ±5 mA injection limit either way.

**The part stays. The sentence needs rewriting**, and the exact abs-max wording re-read from DS12288
Tables 14/15 — flagged for G1 rather than fixed from memory, because that is how defect 8.11 happened.

### Defect 8.21 — the conditioning-cell diagram omitted the switch (MAJOR — 24 instances)

**Found by the user**, reading the diagram and asking why pressing the button wouldn't just short the
net to ground. It would, as drawn. The diagram in `wheel-schematic-complete.md` §5.1 and in the wheel
guide was:

```
switch/encoder contact ──┬── R(1 kΩ) ──┬── MCU pin
                         │             ├── C(100 nF) ── GND
                        GND            └── R(10 kΩ) ── +3V3
```

The vertical stub to `GND` was standing in for the switch — **with no switch symbol on it**. Read
literally, the contact net is hard-wired to ground: the pull-up is permanently loaded, the input reads
LOW forever, and no press ever changes it.

**This is the most-repeated cell on the board — 24 instances**, covering every encoder A, B and SW
line and every button. It is explicitly labelled "make this a device sheet or snippet", so it is
copied rather than re-read, and an error in it propagates to every one of those 24 without a second
look. It is the highest-multiplicity drawing in the project.

The information was not *absent* from the project — §5.2 and §5.3 both say encoder commons and the far
side of each button go to `GND`. But **the diagram is what gets captured**, and a reader reconciling a
diagram against a table will usually trust the diagram, because a picture reads as more specific than
prose.

**Corrected** to draw the switch explicitly between the far end of the 1 kΩ and `GND`, with the two
states tabulated: open → 3.3 V HIGH; closed → the 1 kΩ and 10 kΩ form a divider and the pin sits at
**0.3 V**, not 0 V, against a V_IL of about 0.99 V. Stating that pressed ≠ shorted is the part that
makes the cell make sense, and no version of this document had ever said it.

**Class:** the same family as 8.16 (a designator with no part) and 8.18 (a part with no BOM line) —
**a drawing that omits a component**. All three are things a document *fails to say*, which no
consistency check between documents can find, because the documents agree with each other perfectly.
Both of the previous two were caught by tooling written for something else; **this one needed a human
looking at a picture and asking whether it made sense.**

### Flagged for G1 in the same section — the injection-current wording (no board change)

§5.1 says the series 1 kΩ "limits injected current to <5 mA even on a direct short to 5 V." Two
imprecisions, neither of which changes a part:

1. **The arithmetic gives exactly 5 mA**, not less than it (5 V / 1 kΩ). The real figure is smaller —
   about 1 mA — because the pin clamps near 4 V, so the drop across the resistor is ~1 V, not 5 V. The
   conclusion is right and the stated derivation is not the one that supports it.
2. **It uses positive-injection reasoning**, and DS12288 Table 15 note 3 says positive injection "is
   not possible" on some of these I/O classes — the same wording that drove defect 8.7 on the paddle
   pins. Whether the `BTN`/`ENC` pins are that class has not been checked.

Neither is urgent: the on-board buttons cannot realistically see 5 V, and the two lines that *do*
leave the board (`BTN5`/`BTN6` via `J5`/`J6`) already carry `D7`/`D8` BAV99 clamps to `+3V3` and
`GND`. **Grouped with the §4.3 justification flag** for a single re-read of DS12288 Tables 14/15 at
G1, rather than rewritten from memory — which is how defect 8.11 happened.

## §11 — Bourns encoder datasheets, finally read (Rev B.10)

Both were listed as VERIFIED after a distributor-parametric pass; **neither primary datasheet had been
opened**, and `bourns.com` had 403'd every attempt. It answers a plain `curl` with a browser
user-agent. Both PDFs are now in `hardware/lib/`. The user's question — *"both encoders have extra
pins the instructions don't say what to do with"* — is correct, and the read turned up four more
things.

### Defect 8.22 — five terminals documented as three (MAJOR — every encoder on the board)

§5.2 read: *"A→`ENC1_A`, B→`ENC1_B`, SW→`ENC1_SW`, commons → `GND`"*. Both parts have **five
electrical terminals plus mechanical locating features**:

| | PEC09-2120F-S0012 | PEC11H-4120F-S0020 |
|---|---|---|
| Encoder | `A`, `B`, **`C` = common** | `A CHANNEL`, **`C COMMON`**, `B CHANNEL` |
| Switch | **`D`, `E`** — SPST between them | **2 pins** — SPST |
| Mechanical | locating lug(s) | **2 × Ø2.2 mm** locating posts |

The word doing the damage was **"commons"** — plural, undefined, and covering *both* the encoder
common and, apparently, one side of the switch. `SW` named **one** net for a **two**-terminal switch,
so the second switch terminal had no destination anywhere in the project. A capture from this table
leaves a floating switch pin: the button reads as permanently released, on all six encoders.

**PEC11H's common is the CENTRE terminal** (`A CHANNEL / C COMMON / B CHANNEL`, in that physical
order). Treating it as a common-at-one-end part swaps a channel with the common — which fails
*quietly*, producing nonsense counts that read as a firmware quadrature bug.

Identical in shape to **8.19** (the BAV199's third pin) and **8.21** (the missing switch symbol):
a component described by function rather than by terminal. Three in a row, all found by the user.

### Defect 8.23 — a faceplate dimension that is not in the datasheet (MODERATE — machining)

`wheel-pcb-altium-instructions.md` and the BOM both said the PEC11H needs a **"bushing hole Ø9.5 mm +
anti-rotation slot in the faceplate."** The Bourns drawing shows the bushing as **`M7 × 0.75`** — a
7 mm thread, wanting roughly a 7.1–7.5 mm clearance hole. **Ø9.5 mm is not in the document**, and a
faceplate machined to it leaves the encoder loose in a 2.5 mm oversized hole.

The "anti-rotation slot in the faceplate" is also unsupported: the anti-rotation feature these parts
provide is the **PCB-side locating posts** (2 × Ø2.2 mm on the PEC11H, a lug on the PEC09). If a
faceplate feature is wanted it must be confirmed, not assumed.

Both corrected to cite `M7 × 0.75` and to flag the clearance hole for re-derivation **before the
faceplate is machined** — this is a number that gets cut into metal.

### Environmental findings — two new assumptions

| | PEC09 | PEC11H | The rest of the BOM |
|---|---|---|---|
| **Operating temperature** | **−10…+70 °C** | **−20…+70 °C** | −40…+85 °C |
| Storage | −40…+85 °C | −35…+85 °C | |
| **IP rating** | **IP 40** | **IP 40** | KSC4 buttons are **IP 67** |

**A11 (temperature).** The **+70 °C ceiling** is the live half, not the cold end: this is the same
limit that put the display on assumption **A9**, and the encoders sit on the same sun-facing faceplate.
A9's track-day surface-temperature measurement now covers the encoders too — one measurement, three
parts.

**A12 (ingress).** The buttons were chosen **specifically** for IP67. The encoders are **IP40** —
dust over 1 mm, and **no water protection at all** — on the same panel, on a wheel that sees rain and
sweat. Nothing is wrong with either choice; what was missing is that they were never compared. The
encoders are now the wheel's ingress-limiting parts, and whether that needs sealing boots, a shrouded
faceplate or simply accepting it is a team decision, not a datasheet one.

### Confirmed, not corrected — the conditioning cell holds up

- **Contact bounce is 5.0 ms (PEC09) / 3.0 ms (PEC11H)**, both far longer than the cell's 1 ms rising
  RC. This is fine, and the reason deserves writing down because the numbers look alarming: in
  quadrature the count direction depends on the **relative** state of A and B, so while one contact
  chatters the other is stable and the counter steps up and down alternately, landing back where it
  started. Hardware quadrature decoding is bounce-cancelling by construction. The RC and the timer's
  `IC1F = 0b1111` filter (~1.5 µs at 170 MHz) handle electrical noise, not mechanical bounce — the
  comment in `encoder.c` calling it "filters the illegal state transitions that contact bounce
  produces" describes the wrong mechanism for the right conclusion.
- **The switch lines** go through the software integrator at 20 ms, comfortably over 3–5 ms. ✔
- **Max speed is 60 RPM**, so edges are **21 ms** apart (12 PPR) and **12.5 ms** apart (20 PPR) at the
  rated maximum. §5.1's "~8 ms between edges" is *more* conservative than the part allows, giving the
  1 ms rising τ **12–21×** margin rather than the ~8× implied. Better than documented.
- **300 µA** through the cell against a **10 mA @ 5 V** contact rating; PEC09's 3 Ω closed-circuit
  resistance vanishes against the 1 kΩ series. ✔
- **Bourns' suggested filter is 10 kΩ series + 0.01 µF**, not our 1 kΩ + 100 nF. Ours is deliberate: a
  10 kΩ series against the 10 kΩ pull-up would put the pressed level at **1.65 V**, which is not a
  valid logic low. Recorded so the deviation reads as a decision.
- PEC11H switch force **610 ±306 gf** — firm, which is right for gloved use; flagged for the G3
  gloved-reach test rather than treated as a problem.

### Also corrected in the same pass (documentation, no board consequence)

- The wheel's `D14`/`D15` paddle clamps specified only one diode of the dual BAV199, leaving the third
  pin undefined for whoever captures it. **Both halves are needed**: `D3`/`D4` are *bidirectional*
  SMAJ24CA parts, so a negative transient puts −8.03 V on the pin against a VSS − 0.3 V minimum. Now
  specified upper-and-lower, matching the dash DAQ clamps.
- `dash-schematic-complete.md` §2 still listed the AP2112K among the parts the dash copies unchanged
  from the wheel, three sections above §7 explaining that the AP63203 buck replacing it is *the*
  deliberate difference between the boards.
- `dash-schematic-complete.md` §8 claimed PB4 is `PADDLE_UP_SNS` on the wheel. It is spare there; the
  paddle taps are PB0/PB1.
- Both files still listed `SMAJ5.0A` as unread when §7 had closed it.
- The build guide overstated what `check-consistency.py` covers in three specific ways, including
  claiming it checks the plain-English guides (it does not) and that it covered the AP63203 bootstrap
  capacitor (it did not — **now it does**, which is the one place the overstatement was worth fixing in
  the script rather than the prose).
- Stale counts and statuses in the build guide: "eleven defects" and A6 described as an open layout
  blocker after `PROJECT-LOG.md` had recorded it closed.

### Closed by construction — the ADC sample-time question (was blocked on RM0440)

`AIN_SAMPLE_CYCLES` had carried an `UNVERIFIED` marker since Step 1: the DAQ front end presents a
5 kΩ Thévenin source, and the minimum sample time for that impedance lives in an RM0440 table that
has never been readable — **ST's server has now failed four download attempts**, so this was not going
to resolve by waiting.

**Closed by choosing the extreme instead of looking up the value.** Use the longest sample time the
hardware offers, `SMP = 111 = 640.5 cycles`, on every channel. If that is insufficient for 5 kΩ then
no setting is and the DNP `TLV9004` buffer must be fitted — so the choice cannot be wrong in the
dangerous direction, which is the property an unverified number does not have.

It is affordable, computed from figures that *are* in DS12288:

| Quantity | Value | Source |
|---|---|---|
| f_ADC max | **52 MHz** | Range 1, all ADCs, single-ended, VDDA ≥ 2.7 V |
| Sampling rate | f_ADC / (t_s + resolution + 0.5) | DS12288 ADC characteristics |
| One conversion | (640.5 + 12.5) / 52 MHz = **12.56 µs** | computed |
| Full 8-channel scan | **100 µs** | × 8 |
| Duty at a 100 Hz scan rate | **1.0 %** | computed, and asserted in `test_daq.c` |

Trading 1 % of a timer for an open datasheet question is a good trade. The previous value (92.5
cycles) was written from memory and marked unverified — a liability that does not expire on its own.

### JDI LPM013M126A serial protocol — VERIFIED (was never read)

§6B closed the *electrical* selection of the colour panel but not its **wire protocol**, which was
still unread when the driver was written. The timing charts are images and did not survive text
extraction, so the pages were rendered and read directly. Everything below is now from the spec
(Ver.01 §6.1 single-line update, §6.8 all clear, §4.3 power sequence):

| Fact | Value |
|---|---|
| Frame structure | `M0 M1 M2 M3 M4 M5` (6 clk) · `AG9…AG0` (10 clk) · pixel data · 16 clk dummy |
| Bit order | **MSB first throughout**, including the address |
| Address width | **10 bits**, not 8 |
| Line numbering | **one-based**, "1 to 176" |
| Mode bits, single-line 3-bit update | M0 = H, M2 = L, M3 = L, M4 = L; M1 and M5 don't care |
| Mode bits, all clear (§6.8) | M0 = **L**, M2 = **H** |
| Pixel order | R-G-B within a pixel, pixel 1 first, 3 bits each |
| Line size | 176 × 3 = 528 bits = **exactly 66 bytes**, no padding |
| Multi-line mode | 6 clocks between gate lines, 16 after the last |
| Power-on | T2 ≥ 1 ms memory init · T3 ≥ 30 µs latch release · T4 ≥ 30 µs COM init |
| Supply ordering | "VDD and VDDA should rise simultaneously **or VDD should rise first**" — satisfied because both tie to `+3V3`; a separate filtered VDDA rail would break it |

**The trap this avoided.** The obvious way to write this driver is to reuse a Sharp memory-LCD
driver — they are everywhere, the panel is pin-compatible, and §6B already notes the two parts are
pin-for-pin identical. But the Sharp part sends an **8-bit line address, bit-reversed (LSB first)**.
This panel sends a **10-bit address MSB-first**. A driver carrying that assumption **works** — the
panel accepts every frame and displays cleanly — it simply writes to the wrong lines. Line 1 becomes
line 128. That presents as a UI layout problem, not a bus problem, and the pin-compatibility of the
two parts actively encourages the wrong guess.

Recorded here rather than as a numbered defect because it was caught while writing the driver, before
any code depended on it. The test `test_display.c` now asserts the address is *not* bit-reversed, as
a tripwire for anyone who later pastes in the Sharp version.

### BT817Q EVE protocol and panel timing — VERIFIED (BT81X datasheet obtained)

§5B closed the Riverdi module's *power* architecture but not how to drive it. Both halves are now
read: the **BT81X datasheet BRT_000220 v1.0** was downloaded for the SPI protocol and register map,
and the **Riverdi DS Rev 1.7** supplied the panel timing values.

| Fact | Value | Source |
|---|---|---|
| SPI prefixes | **0b00 read · 0b10 write · 0b01 host command** · 0b11 undefined | §5 |
| Address width | 22 bits, MSB first | §4.1.2 |
| Read vs write | **A read has a dummy byte after the address; a write does not** | §4.1.3 / §4.1.4 |
| Host command | 3 bytes: `[01\|cmd5:0]`, parameter, **0x00 fixed** | §4.1.5 |
| Memory map | RAM_G 0x000000 · ROM 0x200000 · RAM_DL 0x300000 · RAM_REG 0x302000 · RAM_CMD 0x308000 (4 kB) | §5 |
| `REG_ID` | 0x302000, always reads **0x7C** | register table |
| `ACTIVE` command | 0x00 | Table 4-5 |
| Panel timing | HSIZE 800 · VSIZE 480 · HCYCLE 816 · HOFFSET 8 · HSYNC0 0 · HSYNC1 4 · VCYCLE 496 · VOFFSET 8 · VSYNC0 0 · VSYNC1 4 · PCLK 1 · SWIZZLE 0 · PCLK_POL 1 · CSPREAD 0 · DITHER 0 | Riverdi DS, "REGISTER VALUES" |

**A guessed register address was caught here.** A first draft of `display_dash.c` placed `REG_HSIZE`
at `RAM_REG + 0x2C` by inferring even spacing from the base. The real address is `+0x34`; `+0x2C` is
**`REG_HCYCLE`**. Writing 800 into HCYCLE and 480 into HOFFSET would have produced a display that
lights up and shows a torn image — a symptom that reads as a faulty panel. **The register table has
gaps and cannot be walked arithmetically.** All seventeen addresses are now read individually and
asserted in `test_display_dash.c`.

**One item stays [OPEN], and it is the EVE3/EVE4 boundary.** The datasheet obtained covers
**BT815/6 (EVE3)**; the module carries a **BT817Q (EVE4)**. The SPI protocol, memory map and register
addresses are common to both. `REG_PCLK` is not: EVE3 defines it as a straight divisor —
*"PCLK frequency = System Clock frequency / REG_PCLK"* — under which Riverdi's `REG_PCLK = 1` would
run the panel at the full system clock. On EVE4 that value selects the separate `REG_PCLK_FREQ`
register instead. The arithmetic supports the EVE4 reading: 816 × 496 = 404,736 pixel clocks per
frame, so 60 Hz needs **24.3 MHz**, not 72 MHz. Confirm `REG_PCLK_FREQ` against the BT817/818
datasheet or Riverdi's published init sequence before bring-up. Failure mode is loud — no image or a
badly wrong refresh rate — so it is safe to discover on the bench.

### HSE crystal — SELECTED AND VERIFIED (was the last `[OPEN]` on the wheel MCU sheet)

**Selected: Abracon ABM8 series, 16.000 MHz, CL = 8 pF, ESR ≤ 70 Ω, C0 ≤ 3 pF, ±30 ppm tolerance,
±30 ppm stability, −40…+85 °C, 3.2 × 2.5 mm.** Load caps `C_X1`/`C_X2` = **6 pF C0G**.

Sources read: **ABM8 datasheet (Abracon, rev 07-29-20)** for ESR/C0/CL/drive level, and **DS12288
Table 41** for the oscillator side.

#### The startup criterion, and why the obvious answer was wrong

`gm_crit = 4 · ESR · (2πF)² · (C0 + CL)²`, against DS12288 Table 41's **`Gm` = 1.5 mA/V max**.

The intuition "a slower crystal starts more easily" is **backwards for a small package**, because ESR
climbs steeply as frequency falls and `gm_crit` only scales with F². The ABM8's own ESR table:

| F (MHz) | ESR max (Ω) | `gm_crit` @ CL = 8 pF | Headroom |
|---|---|---|---|
| **8** | **400** | 0.489 mA/V | 3.1× |
| 12 | 120 | 0.330 mA/V | 4.5× |
| **16** | **70** | **0.342 mA/V** | **4.4×** ✅ |
| 20 | 50 | 0.382 mA/V | 3.9× |

8 MHz — the "default" STM32 reference frequency, and the one an earlier draft of this file would have
led someone to — is the **worst** of the four. 16 MHz was kept because it is also what the PLL
configuration already assumes (`M = 4` → 4 MHz PLL input), so no firmware changed.

#### CL = 8 pF is pinned from both sides

| CL | `gm_crit` | Headroom | `C_ext = 2(CL − C_stray)` at C_stray ≈ 5 pF |
|---|---|---|---|
| 6 pF | 0.229 mA/V | 6.5× ✅ | **2 pF — stray dominates, not buildable** ❌ |
| **8 pF** | **0.342 mA/V** | **4.4× ✅** | **6 pF ✅** |
| 10 pF | 0.478 mA/V | 3.1× ❌ | 10 pF ✅ |

The transconductance table alone would pick 6 pF; the buildability constraint alone would pick 10 pF.
Only 8 pF satisfies both. **This is why the old "CL 8–12 pF, ESR ≤ 80 Ω" range was not a
specification** — it permitted combinations down to 2.1× headroom, and a reader picking the middle of
each range would have landed on a crystal that starts unreliably cold.

#### Interpretation caveat, stated rather than hidden

DS12288 names the 1.5 mA/V figure *"maximum critical crystal transconductance"*. That reads as "your
crystal's `gm_crit` must be ≤ 1.5", but ST's AN2867 method instead compares the oscillator's `gm`
against `gm_crit` and asks for a **5× gain margin**. **AN2867 could not be retrieved** (st.com timed
out repeatedly, the same failure as RM0440), so the ambiguity is unresolved. The design therefore
targets `gm_crit` ≤ 0.3 mA/V *where achievable*, which satisfies both readings; the selected point is
0.342 mA/V, which satisfies the literal reading outright and sits just under a 5× margin on the
stricter one. **Bench measurement at bring-up is the tie-breaker**, and it is in the staged procedure.

#### Two bench measurements this selection requires

1. **Drive level.** Worst case (full-rail swing assumed at OSC_OUT): `I_rms` = 1.29 mA →
   `DL = I²·ESR` = **117 µW against the ABM8's 100 µW maximum**. The real figure is normally lower —
   the STM32 oscillator has amplitude control — but the estimate lands *above* the limit and cannot be
   assumed away. Overdriving ages a crystal: it drifts over months and eventually fails, i.e. a
   warranty-period failure, not a bring-up one.
2. **Cold start.** `gm_crit` is worst at low temperature. `tSU(HSE)` is 2 ms typ.

**`R_X1` is the adjustment for (1) and it cuts both ways** — it reduces drive level *and* startup
margin. With only 4.4× headroom, any non-zero `R_X1` must be re-verified cold.

#### A circuit error caught while writing this up

The first draft placed `C_X2` **at the MCU pin**, on the near side of `R_X1`. That is wrong: `R_X1`
and `C_X2` together form the low-pass that limits drive, so `C_X2` must sit on the **crystal** side.
With it at the pin, `R_X1` costs startup margin and delivers no drive reduction — and the bench
measurement would not improve no matter what value was fitted, which is a maddening thing to debug.
Corrected: three nets, `NET_OSC_IN` — `NET_OSC_OUT` — `NET_XOUT`.

#### Frequency budget

±30 tolerance + ±30 stability + ±2 aging + ±29 (1 pF load-cap error) = **±91 ppm**. CAN needs
~5 000 ppm → **55× margin**; USB full-speed needs 2 500 ppm → 27×. The load-cap term is the largest
controllable one, which is the practical reason the layout rules demand short oscillator traces.

#### ⚠ The ABM8G is NOT a substitute despite the near-identical name

80 Ω and `C0 ≤ 5 pF` give `gm_crit` = 0.547 mA/V — **2.7× headroom**. Any second source must be
checked against all three numbers: **ESR ≤ 70 Ω, C0 ≤ 3 pF, CL = 8 pF.**

### LQFP-64 VDD pin count — closed, and it was six in the BOM

Both BOMs specified `C16`–`C21`, "6 × 100 nF, one per VDD pin". **Counted off DS12288 Figure 7, the
LQFP-64 has four `VDD` pins — 16, 32, 48 and 64** — plus `VBAT` (1), `VDDA` (29), `VREF+` (28) and
four `VSS` (15, 31, 47, 63). Corrected to `C16`–`C20` = five: one at each VDD, one at VBAT, with
VDDA covered by the existing `C8`/`C9` pair.

A spare capacitor in a BOM is harmless. A *count* asserted as fact and never checked against the
package drawing is not — it gets copied into a layout review as evidence that the decoupling was
thought about. This was flagged as a known guess in the Step 1 plan and had survived since.

### Recorded, not counted — one suspicion needing hardware

`LED_DATA_3V3` (PA6) has no pull-down, and the 74AHCT1G125 buffer has `/OE` hard-tied to GND, so the
buffer drives the WS2812 chain from a **floating CMOS input** between power-on and firmware init. A
10 kΩ pull-down on PA6 is free and removes the question, but the 74AHCT1G125 datasheet has not been
read locally, so no number is asserted here. **SUSPECTED, MINOR** — carried into the bring-up plan.

### Coverage — audited this pass and found clean

Every encoder pair re-checked pin-by-pin against Table 12 (ENC1 PC0/PC1 TIM1 · ENC2 PC6/PC7 TIM3 ·
ENC3 PB6/PB7 TIM4 · ENC4 PA15/PB3 TIM2 · ENC5 PB2/PC2 TIM20 · ENC6 PA0/PC12 TIM5), the
encoder-capable timer list (TIM1/2/3/4/5/8/20 + LPTIM1 per §3.24.3), LED on TIM16_CH1 + DMA via
DMAMUX, SPI1 sets on both boards, FDCAN1 forced to PB8/PB9 by USB, USB FS on PA11/PA12, all eight dash
DAQ pins reaching ADC1, paddle pin class `TT_a` confirmed, PC13 as an *input* (note 2 restricts PC13–15
in output mode only), and the crystal on PF0/PF1 (both bonded on LQFP-64). Power chain re-derived:
53.3 V clamp vs 66 V abs max = 12.7 V margin. CAN chain: PESD2CAN 41 V clamp vs ±58 V bus tolerance,
VIO/VCC split, S to GND, TXD idling recessive with no firmware.
