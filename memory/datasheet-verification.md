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
| STM32G474RET6 | **PARTIAL — see §1.4 for what was *not* read** | **4 defects found** (3 pin-map + missing clock source). See §1 |
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
1. Set the option bits so BOOT0 comes from the **`nBOOT0` option bit, not the pin** (`nBOOT_SEL = 1`),
   as part of first flashing. Verify it on every board.
2. **Delete the "10 kΩ BOOT0 pulldown + test point" from the original instructions** — that circuit
   fights the CAN transceiver's RXD output and is actively harmful here.
3. DFU entry uses the USB DFU path or an SWD-triggered jump, **not** a BOOT0 strap.
4. A full chip erase can restore the factory option-bit state, so **re-check `nBOOT_SEL` after any
   mass erase.** Put it in the bring-up log.

### Corrected wheel pin map — every entry justified by the AF table

| Function | Pin | Justification (from Table 13) |
|---|---|---|
| ENC1 A / B | PC0 / PC1 | TIM1_CH1 / TIM1_CH2 |
| ENC2 A / B | PC6 / PC7 | TIM3_CH1 / TIM3_CH2 |
| ENC3 A / B | PB6 / PB7 | TIM4_CH1 / TIM4_CH2 |
| ENC4 A / B | PA15 / PB3 | TIM2_CH1 / TIM2_CH2 |
| ENC5 A / B | PB14 / PB15 | TIM15_CH1 / TIM15_CH2 |
| ENC6 A / B | PA0 / PC12 | TIM5_CH1 / TIM5_CH2 |
| LED_DATA | PA6 | TIM16_CH1 + DMA — moved off TIM1 (defect 1.2) |
| LCD_SCLK / LCD_SI | PA5 / PA7 | SPI1_SCK / SPI1_MOSI (PB3 alternative is taken by ENC4) |
| LCD_SCS / LCD_DISP / LCD_EXTCOMIN | PA4 / PC2 / PC3 | plain GPIO; EXTCOMIN is a ~1 Hz software toggle, no timer needed |
| CAN_RX / CAN_TX | PB8 / PB9 | FDCAN1 — **only option left after USB claims PA11/PA12** (defect 1.3) |
| USB_DM / USB_DP | PA11 / PA12 | USB FS |
| SWDIO / SWCLK | PA13 / PA14 | |
| DBG_TX / DBG_RX | PA9 / PA10 | USART1 — free now that TIM1 encoder moved to PC0/PC1 |
| V12_SENSE / V5_SENSE | PA1 / PA2 | ADC1_IN3 / ADC1_IN4 |
| PADDLE_UP/DN_SNS | PB0 / PB1 | GPIO input |
| ENC1–6_SW | PC4, PC5, PC8, PC9, PC10, PC11 | GPIO input |
| BTN1–6 | PC13, PD2, PA3, PB10, PB11, PB12 | GPIO input (PD2 = pin 55, confirmed bonded on LQFP-64) |
| Spare | PA8, PB2, PB4, PB5, PB13 | |

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
| ±5 mA pin injection current limit | The DAQ fault analysis (0.8 mA is "safe") | Would change the AFE series resistor |
| ~0.10 A MCU run current at 170 MHz | Both boards' power budgets | Budgets have large margin; low risk |
| ADC max external source impedance / sample-time table | Justifies omitting the op-amp buffer at 5 kΩ | Could force the DNP TLV9004 to be fitted |
| Six VDD pins on LQFP-64 → six 100 nF caps | Decoupling count | Cosmetic; add caps to match the real pin count |
| `nBOOT_SEL = 1` selects the option bit over the pin | The BOOT0 mitigation (defect 1.3) | **Must be confirmed** — the mitigation depends on it |
| Absolute maximum ratings, VDDA sequencing | General | Standard practice covers it |

The `nBOOT_SEL` semantics are the one that matters, because the whole BOOT0 fix rests on them.
Confirm in the reference manual (RM0440) before first flash.

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

**Worth noting:** the 10 kΩ series resistor already does the real protection work. A 12 V fault on a
sensor line injects (12 − 3.3 − 0.7)/10 kΩ ≈ **0.8 mA** into the MCU's internal protection diodes,
comfortably inside the STM32's ±5 mA injection limit. The external clamp is belt-and-braces, which is
exactly why it must not cost accuracy. Keep it, but keep it low-leakage.

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
| 5 V→3.3 V LDO (**both**) | **AP2112K-3.3TRG1** (SOT-25), LCSC `C51118`, ~$0.083 | 600 mA min, dropout 0.25 V @ 600 mA, **explicitly specified for 1 µF X7R/X5R ceramic in and out** | Replaces AMS1117 and its tantalum requirement (§5C). Load ~100 mA ✅ |
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
