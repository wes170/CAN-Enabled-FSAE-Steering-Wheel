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
| STM32G474RET6 | **PARTIAL — see §1.4b** | **6 defects found** (4 pin-map, missing clock source, wrong BOOT0 option-bit name). See §1 |
| Sharp LS013B7DH05 | **PARTIAL** | **1 defect found (EXTMODE omitted).** See §2 |
| TJA1051T/3 | **VERIFIED** | Compliant as designed. See §3 |
| Haltech CAN broadcast protocol | VERIFIED | Read in full; §2.3 table transcribed from it |
| Blink PKP2600SI CANopen | VERIFIED | Read in full; keypad frames transcribed from it |
| WS2812B-2020 | **UNVERIFIED** | Datasheet is image-only, not text-extractable. See §4 |
| AP63205 / AP63203 | **VERIFIED** | **CRITICAL defect — 35 V abs max cannot face a vehicle battery.** See §5 |
| SMBJ33A vs buck abs-max | **VERIFIED** | **CRITICAL — TVS clamps 18 V above what the wheel buck survives.** See §5 |
| LMR33630 | **PARTIAL** | Rejected on abs-max grounds anyway (§5) |
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
Stocked variants are 1 MHz; prefer the **non-PFM (FPWM) variant** for constant-frequency EMI next to
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
| LMR36015 EN | Must not exceed VIN by >0.3 V; abs max 66.3 V | Tied directly to `+12V_P` (= VIN) | ✅ equal, so within spec |
| LMR36015 VIN | Abs max 66 V | SMBJ33A clamps 53.3 V | ✅ 12.7 V margin |
| AP2112K VIN | From 5 V rail | LMR36015 output | ✅ |
| ADC inputs | 0–3.3 V | Divider gives 0–2.5 V from 0–5 V sensors | ✅ 0.8 V headroom |
| Encoder/button lines | 3.3 V logic | 10 kΩ pull-up to `+3V3`, switch to GND | ✅ |
| Paddle sense | ECU-driven line, unknown pull-up voltage | 100 kΩ series + 1 nF, and the MCU pins are **5 V-tolerant FT** | ✅ but confirm the Nexus DI pull-up voltage; if it pulls to 12 V, the 100 kΩ limits current but the pin still needs the clamp |

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
