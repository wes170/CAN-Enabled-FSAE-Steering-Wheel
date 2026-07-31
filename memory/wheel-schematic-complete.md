# FSAE-WHEEL Rev B — Complete Schematic Definition

> **This file is self-contained.** Every part number, pin number, component value and net connection
> needed to draw the wheel schematic in Altium is here. You should not need a datasheet, a
> distributor page, or any other file open while capturing.
>
> Every number here has been read out of a datasheet — see `datasheet-verification.md` for the
> per-part evidence. Where something is genuinely still open it is marked **[OPEN]** and explained,
> rather than filled with a plausible guess.
>
> Companion files: `wheel-pcb-altium-instructions.md` (Altium mechanics, layout, outputs),
> `hardware-selections.md` (why each part), `engineering-rigor.md` (gates).

---

## 0. How to read this document

- **Net names are literal.** Type them exactly as written; the layout rules and the firmware pin map
  both key off them.
- Each section is one schematic sheet. Draw them in the order given.
- `DNP` = place the footprint, do not fit the part. `[OPEN]` = decision not yet closed.
- Designators are pre-allocated and non-overlapping across sheets — use them as given so the BOM,
  the layout notes, and the bring-up log all agree.

## 1. Global nets

| Net | Meaning |
|---|---|
| `+12V_IN` | Raw vehicle 12 V at connector J1 pin 1, before any protection |
| `+12V_P` | Protected 12 V — downstream of fuse, P-FET and TVS. Feeds the buck only |
| `+5V` | Local 5 V rail from U1. Feeds LEDs, CAN transceiver, LED buffer, LDO |
| `+3V3` | Local 3.3 V from U2. Feeds MCU, display, all pull-ups |
| `+3V3A` | Filtered analog 3.3 V — VDDA/VREF+ only |
| `GND` | Single ground net. Split into analog/power *pours* in layout, one net in schematic |

**Power tree:** `+12V_IN → F1 → Q1 → (D1 clamp) → +12V_P → U1 → +5V → U2 → +3V3 → FB1 → +3V3A`

---

## 2. Sheet `wheel-power.SchDoc`

### 2.1 Input protection

| Ref | Part | Value / spec | Connections |
|---|---|---|---|
| `F1` | Resettable polyfuse, 1206 | 1.1 A hold / 2.2 A trip / 30 V | `+12V_IN` → `NET_FUSED` |
| `Q1` | **DMP3056L**, SOT-23, P-channel | V_DSS −30 V, V_GSS ±20 V, I_D −4.3 A | **Drain** → `NET_FUSED` (battery side); **Source** → `+12V_P` (board side); **Gate** → `NET_QGATE` |
| `R1` | Resistor 0402 | 10 kΩ 1 % | `NET_QGATE` → `GND` |
| `D5` | Zener, SOD-323 | 12 V, 500 mW | Cathode → `NET_QGATE`; Anode → `GND` |
| `D1` | **SMBJ33A** TVS unidirectional, SMB | 33 V standoff, V_CL 53.3 V @ 11.26 A | Cathode → `+12V_P`; Anode → `GND` |
| `C5` | Ceramic 0603 | **100 nF, 100 V, X7R** | `+12V_P` → `GND` |

> ### ⚠ Q1 orientation — get this right or the protection does nothing
>
> **Drain faces the battery, source faces the board.** (Equivalently: "drain-to-battery", which is
> how `hardware-selections.md` §3.2 words it, and "source→board" as the Altium instruction files
> word it. All three phrasings mean the same thing.)
>
> **Why:** a P-channel MOSFET's body diode conducts **drain → source**. With the drain at the supply,
> that diode is *forward*-biased in normal operation, so current reaches the board even before the
> channel turns on; the source then sits near +12 V while the gate is held at ground, giving
> V_GS ≈ −12 V, which turns the channel hard on and shorts out the diode drop. On a **reversed**
> supply the body diode is *reverse*-biased and V_GS ≈ 0 V, so both paths are off and nothing gets through.
>
> **Wired the other way round** (source to the battery), the circuit still works perfectly on the
> bench with correct polarity — the FET turns on and current flows. But on a reversed supply the body
> diode becomes forward-biased and dumps the fault straight into the board, **with the schematic
> looking entirely plausible.** That is what makes this error dangerous: it is invisible until the
> day someone connects a battery backwards, which is the one day the part exists for.
>
> *(Corrected 2026-07. An earlier revision of this file had this backwards while the other three
> source documents were right — recorded as defect 5.5 in `datasheet-verification.md`.)*

**Why the zener:** with the gate pulled to ground, V_GS = −V_IN. A load dump clamped at 53 V would put
−53 V on a ±20 V gate. D5 holds it at −12 V; zener current at clamp is (53 − 12)/10 kΩ = 4.1 mA.

### 2.2 U1 — 12 V → 5 V buck, **TI LMR36015**, VQFN-HR-12 (RNX)

Pinout (from datasheet): `1,11 PGND` · `2,10 VIN` · `3 NC` · `4 BOOT` · `5 VCC` · `6 AGND` ·
`7 FB` · `8 PG` · `9 EN` · `12 SW`.

| Ref | Value | Connection |
|---|---|---|
| `U1.2`, `U1.10` | — | `+12V_P` |
| `U1.1`, `U1.11`, `U1.6` | — | `GND` (PGND ×2 and AGND all to GND) |
| `U1.12` (SW) | — | `NET_SW` |
| `U1.3` (NC) | — | **tie to `NET_SW`** — the datasheet says to do this so CBOOT routes cleanly |
| `U1.9` (EN) | — | `+12V_P` (always enabled). **Why straight to VIN and not a divider:** a divider on EN would give a *programmable* start-up threshold (UVLO). We do not need one — the LMR36015 has its own internal UVLO, and there is no sequencing requirement on this board. The datasheet's Pin Functions table says of EN: *"Enable input to regulator. High = ON, low = OFF. **Can be connected directly to VIN; Do not float.**"* — so tying them together is the datasheet's own sanctioned arrangement. ⚠ An earlier revision of this table claimed the datasheet imposes a "must not exceed VIN by more than 0.3 V" rule. **It does not** — that was a misreading of the absolute-maximum table, where VIN-to-PGND (66 V) and EN-to-AGND (66.3 V) are two independent limits referenced to ground, not a relative constraint. Defect 8.11: a fabricated datasheet citation, which is worse than a missing one because it reads as verified |
| `U1.8` (PG) | — | leave open, or `R_PG` 100 kΩ to `+3V3` if you want power-good sensing |
| `C_BOOT` | 100 nF, 25 V, X7R, 0402 | `U1.4` (BOOT) → `NET_SW` |
| `C_VCC` | 1 µF, 16 V, X7R, 0603 | `U1.5` (VCC) → `GND`. **Do not load VCC externally** |
| `L1` | **10 µH**, I_sat ≥ 2 A, shielded | `NET_SW` → `+5V` |
| `C1`,`C2` | **4.7 µF, 100 V, X7R, 1206 (×1) + 220 nF, 100 V, 0402 (×2)** | `+12V_P` → `GND`, one 220 nF at **each** `VIN`–`PGND` pair. See the voltage-rating box below |
| `C3`,`C4`,`C_O3` | **3 × 15 µF**, 16 V, X7R, 0805 | `+5V` → `GND` |
| `R_FBT` | **100 kΩ** 1 %, 0402 | `+5V` → `NET_FB` |
| `R_FBB` | **24.9 kΩ** 1 %, 0402 | `NET_FB` → `GND` |
| `C_FF` | 20 pF, 0402 | across `R_FBT` (`+5V` → `NET_FB`). **This is a feed-forward capacitor** — it puts a zero in the feedback path to improve phase margin and transient response. 20 pF is TI's tabulated value **for this exact divider pair** (100 kΩ / 24.9 kΩ). It is not a value to re-derive or round: if you change `R_FBT`/`R_FBB`, go back to the datasheet table rather than keeping 20 pF |
| `U1.7` (FB) | — | `NET_FB`. **Never float or ground FB** |

> ### ⚠ Every capacitor on `+12V_P` must be rated 100 V, not 50 V (defect 8.10)
>
> `D1` is an SMBJ33A and it **clamps at 53.3 V**. Everything downstream of it sees that during a load
> dump — which is not an edge case, it is the event the TVS exists to handle. A 50 V capacitor on this
> rail is therefore under-rated against the design's own worst case.
>
> The LMR36015 datasheet (§10.2.1.2.6) asks for input capacitors *"rated for at least the maximum input
> voltage that the application requires; **preferably twice** the maximum input voltage"*, and states
> outright that *"the 220 nF must also be rated at **100 V** with an X7R dielectric."*
>
> This is defect 5.1 repeated one component further along: **sizing against the nominal rail instead of
> the clamp.** The buck was raised to a 66 V part precisely because 53.3 V got through — and then the
> capacitors sitting on the same node were left at 50 V. Applies to `C1`, `C2` and `C5` on the wheel and
> the equivalent parts on the dash.
>
> (X7R also loses a large fraction of its capacitance under DC bias. Specifying 100 V here buys
> retained capacitance at 12 V as well as survival at 53 V.)

Values are TI Table 10-1, **1 MHz** row, 5 V output — correct for the recommended part.

**Variant choice (closed).** TI's Device Comparison Table:
`LMR36015FB` = FPWM **yes**, 1 MHz · `LMR36015B` = FPWM no, 1 MHz · `LMR36015A` = FPWM no, **400 kHz**.
**Use `LMR36015FBRNXR`**: forced PWM holds the switching frequency constant at all loads, which is
what you want beside an analog front end and LED drivers. (An earlier note here recommended the "B"
part *for* FPWM — that was backwards, B is the non-FPWM one.)

⚠ **If you instead buy `LMR36015AQRNXRQ1`** — the AEC-Q100 automotive part LCSC stocks — it is the
**400 kHz, non-FPWM** variant, and the passives must change to **`L1 = 15 µH`, `C_OUT = 3 × 22 µF`**.
Automotive qualification is a real benefit; just make it a decision rather than an accident of stock.

### 2.3 U2 — 5 V → 3.3 V LDO, **AP2112K-3.3TRG1**, SOT-25 (LCSC C51118)

| Ref | Value | Connection |
|---|---|---|
| `U2` VIN | — | `+5V` |
| `U2` EN | — | tie to `+5V` (always on) |
| `U2` VOUT | — | `+3V3` |
| `U2` GND | — | `GND` |
| `C6` | 1 µF, 16 V, X7R, 0603 | `+5V` → `GND` at the input pin |
| `C7` | 1 µF, 16 V, X7R, 0603 | `+3V3` → `GND` at the output pin |
| `C7b` | 100 nF, 0402 | `+3V3` → `GND`, as close to the output pin as possible |

**Ceramic is correct here** — the AP2112K is specified for X7R/X5R ceramic in and out. (This part
replaced the AMS1117 precisely because that one requires tantalum; see `datasheet-verification.md` §5C.)

### 2.4 Analog rail and rail monitors

| Ref | Value | Connection |
|---|---|---|
| `FB1` | Ferrite bead, 600 Ω @ 100 MHz, ≥500 mA, 0603 | `+3V3` → `+3V3A` |
| `C8` | 1 µF, 0603 | `+3V3A` → `GND` |
| `C9` | 100 nF, 0402 | `+3V3A` → `GND`, within 5 mm of the VDDA pin |
| `R2` | 47 kΩ 1 %, 0402 | `+12V_P` → `V12_SENSE` |
| `R3` | 10 kΩ 1 %, 0402 | `V12_SENSE` → `GND` |
| `C10` | 100 nF, 0402 | `V12_SENSE` → `GND` |
| `R4` | 10 kΩ 1 %, 0402 | `+5V` → `V5_SENSE` |
| `R5` | 10 kΩ 1 %, 0402 | `V5_SENSE` → `GND` |
| `C11` | 100 nF, 0402 | `V5_SENSE` → `GND` |

Divider maths: 12 V × 10/57 = 2.11 V and 5 V × ½ = 2.5 V, both inside the 3.3 V ADC range.

### 2.5 Test points
`TP1 = +12V_P` · `TP2 = +5V` · `TP3 = +3V3` · `TP4`, `TP5 = GND` (two, spaced as a scope-spring loop).

---

## 3. Sheet `wheel-mcu.SchDoc`

**U4 = STM32G474RET6, LQFP-64** (LCSC C521608).

### 3.1 Complete pin assignment — **all 64 pins, in package order**

Pin numbers are from **DS12288 Rev 6, Figure 7 "STM32G474xB/xC/xE LQFP64 pinout"** (top view).
Every physical pin of the package appears below exactly once. Type the net names literally.

**Nothing is left implicit: if a pin has no net, the row says what to do with it.**

| Pin | Pin name | Net | Notes |
|---|---|---|---|
| 1 | `VBAT` | `+3V3` | No coin cell; RTC unused. **Must not float** — tie to `+3V3`. `C16` 100 nF at the pin |
| 2 | `PC13` | `BTN1` | Conditioning cell §5.1 |
| 3 | `PC14-OSC32_IN` | *no connect* | No LSE crystal fitted. Leave the pad open; firmware configures it as analog |
| 4 | `PC15-OSC32_OUT` | *no connect* | As above |
| 5 | `PF0-OSC_IN` | `NET_OSC_IN` | `Y1` pin 1 and `C_X1` — §3.2 |
| 6 | `PF1-OSC_OUT` | `NET_OSC_OUT` | → `R_X1` → `NET_XOUT` → `Y1` pin 3; `C_X2` on `NET_XOUT` — §3.2 |
| 7 | `PG10-NRST` | `NRST` | `C23` 100 nF to `GND`. **No external pull-up** — there is one inside |
| 8 | `PC0` | `ENC1_A` | TIM1_CH1 |
| 9 | `PC1` | `ENC1_B` | TIM1_CH2 |
| 10 | `PC2` | `ENC5_B` | TIM20_CH2 |
| 11 | `PC3` | `LCD_EXTCOMIN` | ~1 Hz software toggle. **Stops toggling → panel destroyed** (§7) |
| 12 | `PA0` | `ENC6_A` | TIM5_CH1 |
| 13 | `PA1` | `V12_SENSE` | **ADC12_IN2** — 47 k/10 k divider from `+12V_P` |
| 14 | `PA2` | `V5_SENSE` | **ADC1_IN3** — 10 k/10 k divider from `+5V` |
| 15 | `VSS` | `GND` | |
| 16 | `VDD` | `+3V3` | `C17` 100 nF at the pin |
| 17 | `PA3` | `BTN3` | |
| 18 | `PA4` | `LCD_SCS` | GPIO, **active HIGH** — cannot be SPI1 hardware NSS (§7) |
| 19 | `PA5` | `LCD_SCLK` | SPI1_SCK, **≤ 2 MHz** |
| 20 | `PA6` | `LED_DATA_3V3` | TIM16_CH1 + DMA → `U6` level shifter |
| 21 | `PA7` | `LCD_SI` | SPI1_MOSI |
| 22 | `PC4` | `ENC1_SW` | |
| 23 | `PC5` | `ENC2_SW` | |
| 24 | `PB0` | `PADDLE_UP_SNS` | **ADC1_IN15**, `TT_a` 4.0 V abs max. Divider + `D14` clamp — §4.3 |
| 25 | `PB1` | `PADDLE_DN_SNS` | **ADC1_IN12**, same treatment, `D15` |
| 26 | `PB2` | `ENC5_A` | TIM20_CH1 |
| 27 | `VSSA` | `GND` | Analog ground — one point back to `GND` |
| 28 | `VREF+` | `+3V3A` | |
| 29 | `VDDA` | `+3V3A` | Via `FB1`. `C8` 1 µF + `C9` 100 nF within 5 mm. **VDDA must never exceed VDD** |
| 30 | `PB10` | `BTN4` | |
| 31 | `VSS` | `GND` | |
| 32 | `VDD` | `+3V3` | `C18` 100 nF at the pin |
| 33 | `PB11` | `BTN5` | |
| 34 | `PB12` | `BTN6` | |
| 35 | `PB13` | `LCD_DISP` | H = show memory, L = black (image retained) |
| 36 | `PB14` | **spare** | Bring to a test point |
| 37 | `PB15` | **spare** | Bring to a test point |
| 38 | `PC6` | `ENC2_A` | TIM3_CH1 |
| 39 | `PC7` | `ENC2_B` | TIM3_CH2 |
| 40 | `PC8` | `ENC3_SW` | |
| 41 | `PC9` | `ENC4_SW` | |
| 42 | `PA8` | **spare** | Bring to a test point |
| 43 | `PA9` | `DBG_TX` | USART1_TX → `J4`. ⚠ Also `UCPD1_DBCC1` — see §9.1 |
| 44 | `PA10` | `DBG_RX` | USART1_RX → `J4`. ⚠ Also `UCPD1_DBCC2` — see §9.1 |
| 45 | `PA11` | `USB_DM` | To `U5` USBLC6 |
| 46 | `PA12` | `USB_DP` | To `U5` USBLC6 |
| 47 | `VSS` | `GND` | |
| 48 | `VDD` | `+3V3` | `C19` 100 nF at the pin |
| 49 | `PA13` | `SWDIO` | TC2030 pad |
| 50 | `PA14` | `SWCLK` | TC2030 pad |
| 51 | `PA15` | `ENC4_A` | TIM2_CH1 |
| 52 | `PC10` | `ENC5_SW` | |
| 53 | `PC11` | `ENC6_SW` | |
| 54 | `PC12` | `ENC6_B` | TIM5_CH2 |
| 55 | `PD2` | `BTN2` | |
| 56 | `PB3` | `ENC4_B` | TIM2_CH2 |
| 57 | `PB4` | **spare** | ⚠ If ever used, it is `UCPD1_CC2` — see §9.1 |
| 58 | `PB5` | **spare** | Bring to a test point |
| 59 | `PB6` | `ENC3_A` | TIM4_CH1. ⚠ `UCPD1_CC1` — firmware **must** set `PWR_CR3.UCPD1_DBDIS` (§9.1) |
| 60 | `PB7` | `ENC3_B` | TIM4_CH2 |
| 61 | `PB8-BOOT0` | `CAN_RX` | FDCAN1_RX. ⚠ **Fit nothing else on this pin** — defect 1.3 |
| 62 | `PB9` | `CAN_TX` | FDCAN1_TX |
| 63 | `VSS` | `GND` | |
| 64 | `VDD` | `+3V3` | `C20` 100 nF at the pin |

#### Power-pin summary — and a correction

Counted off Figure 7, the LQFP-64 has **exactly four `VDD` pins (16, 32, 48, 64)** and **four `VSS`
pins (15, 31, 47, 63)**, plus `VBAT` (1), `VDDA` (29), `VREF+` (28) and `VSSA` (27).

| Ref | Value | Placement |
|---|---|---|
| `C16` | 100 nF, 0402 | at `VBAT`, pin 1 |
| `C17` | 100 nF, 0402 | at `VDD` pin 16 |
| `C18` | 100 nF, 0402 | at `VDD` pin 32 |
| `C19` | 100 nF, 0402 | at `VDD` pin 48 |
| `C20` | 100 nF, 0402 | at `VDD` pin 64 |
| `C22` | 4.7 µF, 16 V, X7R, 0805 | one per board, near the MCU |
| `C8`, `C9` | 1 µF + 100 nF | at `VDDA`, pin 29 (already in §2.5) |
| `C23` | 100 nF, 0402 | at `NRST`, pin 7 |

⚠ **This supersedes the earlier "`C16`–`C21`: 6 × 100 nF, one per VDD pin".** There are four VDD
pins, not six — the count was a placeholder that had never been checked against the package drawing,
and it is now `C16`–`C20` = **five** 100 nF (four VDD plus VBAT). The decoupling capacitor nearest
each VDD pin is the one that matters; a spare part in the BOM is harmless, but a *count* stated as
fact and never verified is the kind of number that gets copied into a layout review as evidence.

#### Placement rules
- Every 100 nF sits **on the same side of the board as its pin**, with its own ground via. A shared
  via between two decoupling caps re-introduces the inductance the cap exists to remove.
- `C22` bulk near the MCU but not in place of the per-pin caps.
- `VDDA`'s `FB1` ferrite and its two capacitors form the analog supply filter — keep that loop tight
  and do not route digital signals through it.


### 3.2 HSE crystal — **selected and verified**

`PF0-OSC_IN` = **LQFP-64 pin 5**, `PF1-OSC_OUT` = **pin 6**. Both are bonded on this package.

| Ref | Part / value | Connection |
|---|---|---|
| `Y1` | **Abracon `ABM8-16.000MHZ-8-D4Y-T`** — 16.000 MHz, CL = 8 pF, ESR ≤ 70 Ω, C0 ≤ 3 pF, −40…+85 °C, ±30 ppm tol, ±30 ppm stability. SMD 3.2 × 2.5 × 0.8 mm. **Every field after the frequency is a non-standard option — see the ordering box below** | pin 1 → `NET_OSC_IN` (PF0), pin 2 → `GND`, pin 3 → `NET_XOUT`, pin 4 → `GND` |
| `C_X1` | **6 pF ±0.25 pF, C0G/NP0, 0402** | `NET_OSC_IN` → `GND` |
| `C_X2` | **6 pF ±0.25 pF, C0G/NP0, 0402** | **`NET_XOUT` → `GND` — the CRYSTAL side of `R_X1`, not the MCU pin** |
| `R_X1` | **0 Ω, 0402 — fit the footprint, and see the drive-level box** | series `NET_OSC_OUT` (PF1, pin 6) → `NET_XOUT` → `Y1` pin 3 |

No external feedback resistor: DS12288 Table 41 gives an **internal `R_F` of 200 kΩ typ**.

> ### ⚠ Ordering the ABM8 — the default part is the wrong part (defect 8.14)
>
> The ABM8 datasheet (rev. 07-29-20) read first-hand. **Standard specifications** — what you get if
> you order "an ABM8, 16 MHz" and leave the option fields blank:
>
> | Parameter | ABM8 **standard** | What this design needs | Option code |
> |---|---|---|---|
> | Load capacitance `CL` | **18.0 pF** | **8 pF** | `8` (field accepts any CL ≥ 6 pF) |
> | Operating temperature | **−10…+60 °C** | **−40…+85 °C** | `D` |
> | Frequency tolerance @25 °C | ±50 ppm | ±30 ppm | `4` |
> | Frequency stability over temp | ±50 ppm | ±30 ppm | `Y` |
> | Packaging | bulk | tape & reel, 1 k/reel | `T` |
> | Height | 0.80 max | 0.80 max ✓ | blank (`ABM8`, not `ABM81`/`ABM82`) |
> | ESR @ 16.000–19.999 MHz | **70 Ω** ✓ | ≤ 70 Ω | blank — standard |
> | Shunt capacitance `C0` | **3.0 pF max** ✓ | ≤ 3 pF | not an option; standard |
> | Drive level | **100 µW max**, 10 µW typ | see the drive-level box | not an option |
> | Aging, first year | **±2 ppm** ✓ | ±2 ppm | not an option |
>
> Ordering format: `ABM8[height]-[freq]MHZ-[CL]-[custom ESR]-[temp][tol][stab]-[packaging]`, blank
> fields omitted. Hence **`ABM8-16.000MHZ-8-D4Y-T`**.
>
> **Two of those defaults are load-bearing, not preferences.**
>
> - **CL 18 pF instead of 8 pF collapses startup margin to 1.2×** — `gm_crit` = 4·70·(2π·16 MHz)²·
>   (3 + 18 pF)² = **1.248 mA/V against the MCU's 1.5 mA/V**. That is a board that may simply not
>   oscillate, and the fault is intermittent and temperature-dependent when it half-works. The load
>   caps would also have to change from 6 pF to 26 pF, so a wrong-CL part fitted to this PCB is wrong
>   twice over.
> - **−10…+60 °C is not a vehicle rating.** Every other active part in this BOM is −40…+85 °C.
>
> The tolerance/stability defaults are *not* in that class: ±50/±50 ppm gives a ±131 ppm budget, still
> 38× CAN's requirement. They are specified at ±30 ppm because the option is nearly free, not because
> ±50 ppm would fail. **Do not let that leniency bleed onto CL and temperature.**
>
> ⚠ **This is a live buying hazard, like the LMR36015 variant (8.12).** The decision is closed; the
> risk of a purchaser reading "Abracon ABM8, 16 MHz" and ordering the stocked standard part is not.
> Order by the full option string, and check the CL and temperature fields on what arrives.

> **`C_X2` goes on the far side of `R_X1`, and that is not arbitrary.** `R_X1` and `C_X2` together
> form the low-pass that limits how hard the MCU drives the crystal — that is the entire mechanism by
> which a series resistor reduces drive level. Put `C_X2` on the MCU pin instead and `R_X1` is left in
> series with nothing but the crystal, where it still costs startup margin but no longer limits drive.
> You would then have paid the price of the resistor without getting the benefit, and the drive-level
> measurement below would not improve no matter what value you fitted.
>
> Three nets, not two: `NET_OSC_IN` (pin 5 · `Y1`.1 · `C_X1`) — `NET_OSC_OUT` (pin 6 · `R_X1`) —
> `NET_XOUT` (`R_X1` · `Y1`.3 · `C_X2`).

**Why a crystal is mandatory — the internal RC is not good enough for 1 Mbit/s CAN.** HSI16 is
**−1 %/+1 % over 0…85 °C** and **−2 %/+1.5 % over −40…125 °C** (Table 43). CAN tolerates roughly
**±0.5 %** per node, and that budget is shared with every other node — two nodes each 1 % off are 2 %
apart. Running on HSI16 gives intermittent error frames and bus-off events that worsen as the car
heats up: "perfect on the bench, broken in the car", and it looks like a software fault.

#### Why 16 MHz and CL = 8 pF, specifically

The startup criterion is the **critical transconductance**:

> `gm_crit = 4 · ESR · (2πF)² · (C0 + CL)²`

DS12288 Table 41 gives **`Gm` = 1.5 mA/V max**, "maximum critical crystal transconductance". The
whole selection is a fight between two terms that pull in opposite directions:

- `gm_crit` scales with **F²**, so a *lower* frequency helps.
- ESR rises steeply as frequency falls in a small package, so a lower frequency *hurts*.

Real ABM8 numbers show where the optimum actually is. These are the datasheet's own
**Table 1 — Standard ESR**, read first-hand from the PDF (rev. 07-29-20), not a distributor
parametric summary; every row below re-derives from it exactly:

| F (MHz) | ESR max (Ω) | ESR·F² (relative) | `gm_crit` at CL = 8 pF | Headroom vs 1.5 mA/V |
|---|---|---|---|---|
| 8 | **400** | 25 600 | 0.489 mA/V | 3.1× |
| 12 | 120 | 17 280 | 0.330 mA/V | 4.5× |
| **16** | **70** | 17 920 | **0.342 mA/V** | **4.4×** |
| 20 | 50 | 20 000 | 0.382 mA/V | 3.9× |

**8 MHz is the worst choice, not the best** — the intuition that "slower is easier to start" is
backwards here, because a 3225 crystal at 8 MHz is specified at 400 Ω. 12–16 MHz is the sweet spot,
and 16 MHz is chosen because it is already what the PLL configuration assumes (`M = 4` → 4 MHz PLL
input), so the firmware needs no change.

**CL = 8 pF is forced from both sides**, which is why it is a specific number and not a range:

| CL | `gm_crit` | Headroom | `C_ext = 2·(CL − C_stray)` at C_stray = 5 pF |
|---|---|---|---|
| 6 pF | 0.229 mA/V | 6.5× ✅ | **2 pF — not buildable** ❌ |
| **8 pF** | **0.342 mA/V** | **4.4× ✅** | **6 pF ✅** |
| 10 pF | 0.478 mA/V | 3.1× ❌ | 10 pF ✅ |

6 pF gives the best startup margin and is the value you would pick from the transconductance table
alone — but the STM32's pin capacitance plus trace is ~5 pF, so the external capacitors come out at
2 pF, where stray capacitance dominates the value and the frequency is set by PCB tolerance rather
than by the design. 10 pF is comfortably buildable but fails the startup criterion. **Only 8 pF
satisfies both.**

> #### ⚠ Two things that must be measured on the first board, not assumed
>
> **1. Drive level.** Worst case, assuming the oscillator swings the full rail at `OSC_OUT`:
> `I_rms = 2πF(C0+CL)·V_rms` = 1.29 mA, so `DL = I²·ESR` = **117 µW against the ABM8's 100 µW
> maximum.** The real figure is normally lower — the STM32's oscillator has internal amplitude
> control and does not swing rail-to-rail in steady state — but the estimate lands *above* the limit,
> so it cannot be waved through. **`R_X1` exists for this.** Fit 0 Ω, measure, and if drive level is
> over 100 µW raise `R_X1` (typically 100 Ω – 1 kΩ) and re-check. Overdriving a crystal ages it: the
> frequency drifts over months and the part eventually fails, which is a warranty-period failure, not
> a bring-up failure.
>
> **⚠ `R_X1` cuts both ways.** It reduces drive level *and* reduces startup margin, and the headroom
> here is 4.4×, not 10×. Any non-zero `R_X1` must be re-verified for startup at −40 °C, not just for
> drive level at room temperature.
>
> **2. Startup.** DS12288 gives `tSU(HSE)` = **2 ms typ**. Confirm the oscillator starts at the cold
> extreme, which is where `gm_crit` is worst. Failure to start is loud (the firmware's `clock_init()`
> returns false on HSE timeout and refuses to bring CAN up), so this is safe to discover on a bench.

#### Frequency budget — 55× more accuracy than CAN needs

| Contribution | ppm |
|---|---|
| Initial tolerance @ 25 °C | ±30 |
| Stability over −40…+85 °C | ±30 |
| Aging, first year (ABM8: ±2 ppm) | ±2 |
| Load-cap error, assuming C_stray is off by 1 pF | ±29 |
| **Total worst case** | **±91 ppm = 0.0091 %** |

CAN at 1 Mbit/s needs about **5 000 ppm** per node → **55× margin**. USB full-speed needs
**2 500 ppm** → **27× margin**. The load-cap term is the largest controllable one, which is the
practical reason to keep the `OSC_IN`/`OSC_OUT` traces short: stray capacitance is a *frequency* error
here, not just a startup question.

**USB does not force this decision, and — correcting what this section used to say — USB cannot use
the crystal at all (defect 8.17).** This paragraph previously read *"since CAN requires a crystal
anyway, clock both from the HSE and delete a whole class of clock-accuracy questions."* The first
half is right. The second half is arithmetically impossible on this part:

| | Requirement | Implied VCO |
|---|---|---|
| USB FS needs 48 MHz on PLL"Q" | VCO = 48 × Q, Q ∈ {2,4,6,8} | {96, 192, 288, **384**} MHz |
| SYSCLK is 170 MHz on PLL"R" | VCO = 170 × R, R ∈ {2,4,6,8} | {**340**, 680, …} MHz |

**The two sets do not intersect**, and everything above 344 MHz breaks the VCO ceiling anyway
(Table 46). There is no PLL configuration that serves 170 MHz SYSCLK and 48 MHz USB simultaneously —
it is not a tuning problem. **USB runs on HSI48 + CRS**, trimmed against the host's 1 kHz SOF
packets, which lands far inside USB FS's 2500 ppm without the crystal being involved.

The crystal still earns its place; CAN was always the real justification. What was wrong was the
tidy-sounding claim that one clock source settles both questions. `clock_config.h` now carries the
arithmetic and a static assertion that fires if a future PLL re-tune ever *does* make PLLQ viable, so
the reasoning is guarded rather than just the result. This matters more since Rev B.5, because USB is
a first-class mode on every board rather than a sim-variant feature.

#### Layout rules for this circuit

1. `Y1`, `C_X1`, `C_X2` and `R_X1` go **as close to pins 5/6 as physically possible** — DS12288 says
   so explicitly, "in order to minimize output distortion and startup stabilization time".
2. **A ground guard under and around the crystal**, tied to `GND` with vias, and no signal routed
   under the crystal on any layer. `OSC_IN`/`OSC_OUT` are the highest-impedance nets on the board.
3. Keep the `+5V` buck's `NET_SW` node and the LED data line away from this corner. The buck switches
   at 1 MHz with fast edges; coupling into a 16 MHz oscillator shows up as jitter, and CAN bit timing
   is the thing that notices first.
4. `C_X1`/`C_X2` grounds return to the same ground pour point, not to separate vias — a split return
   puts the two load capacitors at different potentials.

#### Second source

Any 3.2 × 2.5 mm 16.000 MHz crystal is acceptable **provided it is checked against the same three
numbers**: `ESR ≤ 70 Ω`, `C0 ≤ 3 pF`, `CL = 8 pF`. Those three are what the analysis above depends
on. In particular the **ABM8G is *not* a drop-in** despite the similar name — it is 80 Ω and
`C0 ≤ 5 pF`, which gives `gm_crit` = 0.547 mA/V and only **2.7× headroom**.

Note that `C0 ≤ 3 pF` and `100 µW` are *fixed properties of the ABM8*, not order options — a second
source has to be re-checked on those two, whereas on the ABM8 itself only `CL` and temperature are
selectable and therefore gettable wrong (see the ordering box in §3.2).

### 3.3 Reset, boot, debug

| Ref | Value | Connection |
|---|---|---|
| `C23` | 100 nF, 0402 | `NRST` → `GND` |
| — | — | `NRST` also to Tag-Connect pin |
| **BOOT0** | **no component** | ⚠ **Fit nothing.** `PB8-BOOT0` is also `FDCAN1_RX`; a pulldown fights the transceiver output. BOOT0 comes from the **`nBOOT0` option bit (**`nSWBOOT0 = 0` + `nBOOT0 = 1`, via STM32CubeProgrammer**)**, set at first flash and re-checked after any mass erase. See `datasheet-verification.md` defect 1.3 |
| `J_SWD` | Tag-Connect **TC2030-CTX** footprint (copper + 3 locating holes, no part) | pin 1 `+3V3`, 2 `SWDIO`(PA13), 3 `NRST`, 4 `SWCLK`(PA14), 5 `GND`, 6 NC |
| `J4` | JST-GH 3-pin, `SM03B-GHS-TB` | 1 `DBG_TX`(PA9), 2 `DBG_RX`(PA10), 3 `GND` |

### 3.4 USB-C (sim-rig use and DFU)

| Ref | Part / value | Connections |
|---|---|---|
| `J2` | **Molex `204711-0001`**, USB-C receptacle, **vertical (board-perpendicular) mounting** | multi-part symbol — see below |
| `U5` | **USBLC6-2SC6**, SOT-23-6, LCSC `C7519` | I/O1 ↔ `USB_DM_CON`, I/O2 ↔ `USB_DP_CON`, VBUS pin → `NET_VBUS`, GND → `GND`; protected side → `USB_DM`/`USB_DP` |
| `R9`,`R10` | 5.1 kΩ 1 %, 0402 | `CC1` → `GND`, `CC2` → `GND` (sets UFP, 500 mA default) |
| `D6` | **BAT60A** Schottky, SOD-123 | Anode `NET_VBUS` → Cathode `NET_VBUS_OR` |
| `R_VBUS` | 0 Ω, 0603 — **FITTED ON EVERY BOARD (Rev B.5)** | `NET_VBUS_OR` → `+5V` |
| `R11` | 1 MΩ, 0402 | `NET_SHIELD` → `GND` |
| `C24` | 4.7 nF, 0402 | `NET_SHIELD` → `GND` (parallel with `R11`) |

`J2` D+/D− (both pairs — A6/A7 with B6/B7 — tied) → `USB_DM_CON`/`USB_DP_CON`. VBUS pins →
`NET_VBUS`. Signal grounds → `GND`.

**`J2` is a multi-part symbol.** Sub-part `J2A` carries only the six mechanical tabs, `MNT 1`–`MNT 6`;
the USB signals are on the other sub-part(s). **Place every sub-part** — a signal sub-part left
unplaced is pins that exist in the component and appear nowhere on the sheet.

#### `NET_SHIELD` — the shell tabs do NOT go straight to `GND`

| Where | Net |
|---|---|
| `J2A` `MNT 1`–`MNT 6` (shell / mounting tabs) | **`NET_SHIELD`** |
| `NET_SHIELD` → `GND` | only through `R11` 1 MΩ ∥ `C24` 4.7 nF |
| `J2` **signal** ground pins | `GND` directly — these are a different thing from the shell |

Tying the tabs to `GND` shorts out `R11`/`C24` and defeats the choice they exist to make. The net had
no literal name until Rev B.6 — §0 of this file says net names are typed exactly as written, and
"shield" in prose is not a name you can type consistently.

⚠ **Confirm on the Molex drawing that all six tabs are electrically common to the shell.** If any is a
purely mechanical anchor with no connection, it does not belong on `NET_SHIELD`.

#### Why the vertical part, and what it costs

**Chosen by the user (2026-07) for packaging:** a vertical receptacle exits perpendicular to the
board, which is easier to fit behind the wheel faceplate than a right-angle part that needs a clear
run to a board edge. That is a mechanical decision made by the person holding the mechanical
constraint, and it is the right basis for it.

It replaces the previous `LCSC C165948` (HRO `TYPE-C-31-M-12`). Three consequences follow, and none
is a reason to reverse the choice — they are work the change creates:

1. **⚠ Sourcing changes, and this one has teeth.** The old part was an LCSC line, and this project
   assembles at **JLCPCB, which sources from LCSC**. A Molex part number is a Digi-Key/Mouser line.
   If it is not in LCSC's catalogue, `J2` becomes a **consigned or hand-fitted part**, which is a
   different assembly order, not just a different line item. **Check LCSC for the Molex part before
   the BOM is frozen** — see the procurement action in `PROJECT-LOG.md` §3.
2. **Z-height against the faceplate.** A vertical receptacle stands proud of the board, and the mated
   plug stands much further. This lands on **G2** (3D collision against the faceplate STEP) and
   **G3** (1:1 paper build on the real wheel) — with the specific question: *can a USB cable actually
   be inserted and removed with the wheel assembled, or is this a bench-only port?* Either answer is
   fine; discovering it after the faceplate is machined is not.
3. **Withdrawal force is the load case, not insertion.** Plugging in pushes the connector *into* the
   board, which the PCB takes well. **Pulling out peels the pads upward**, which is the direction SMT
   joints are weakest. Six mounting tabs suggests the part is designed for this — **confirm whether
   they are through-hole or SMT.** Through-hole tabs make this a non-issue; SMT tabs make it a layout
   concern worth extra copper and a keep-out for flex.

#### `[OPEN — needs the Molex drawing]` — `J2` pad map

**The pin-to-pad mapping for `204711-0001` has not been read**, and neither had the HRO part's before
it. This file cannot yet tell you which pad is `CC1` versus `CC2`, or how the library symbol numbers
the signal pins across its sub-parts. That is exactly what mirrors a footprint.

**This blocks G6** under rule L35 — placeholder text in a build-from document stops the pre-order
gate, which is the system working rather than an inconvenience. To close it: read the Molex product
drawing, transcribe the pad map into this section as a table, and confirm the library footprint's pad
numbering matches it rather than assuming the vendor symbol is right.

**The USB OR-diode feeds `+5V`, downstream of the buck** — never `+12V_P`.

### 3.4a One build, two cables (Rev B.5)

`R_VBUS` used to be DNP on car boards and fitted only on sim boards. **It is now fitted on every
board**, and the CAR/SIM assembly variants are deleted: every part is populated on every wheel, and
the only difference between a car wheel and a sim wheel is which cable is plugged in. `D6` is what
makes that safe in the normal direction — a Schottky pointing `NET_VBUS` → `NET_VBUS_OR`, so board
power can never reach the host's port.

The remaining DNPs (`R_T1`/`R_T2`/`C_T1` CAN termination, `J7`–`J10`/`D9`–`D12` satellite encoders,
`R_EXTMODE_L`) stay DNP. Those are **installation and provision options, not build variants** — the
distinction matters, because a variant forks the BOM and an option does not.

> ### ⚠ USB back-feeds the vehicle rail through the buck (defect 8.15)
>
> Fitting `R_VBUS` on a board that also has the 12 V front end fitted creates a path that existed in
> neither variant before, because each variant was missing one half of it:
>
> ```
> VBUS 5.0 V → D6 (~0.3 V) → +5V ≈ 4.7 V → L1 → SW
>            → LMR36015 high-side body diode (SW→VIN, ~0.7 V) → +12V_P ≈ 4.0 V
>            → Q1 turns on (V_GS = −4.0 V vs V_GS(th) −2.1 V) → NET_FUSED → F1 → J1.1
> ```
>
> The buck's high-side switch is a bootstrapped NMOS, so its body diode points `SW` → `VIN` and
> conducts whenever the output is driven while `VIN` is dead. Nothing in the design blocks it.
>
> | Situation | What happens | Verdict |
> |---|---|---|
> | USB only, `J1` unmated | ~4 V on an unmated connector pin | harmless |
> | Both connected, car live | buck holds 5.0 V; `D6` reverse-biased at 4.7 V | no conflict |
> | **Both connected, car off** | USB tries to energise the vehicle 12 V bus; the port current-limits | **the real one** |
>
> **Not fixable cheaply, and the obvious fix is wrong.** A series Schottky on the buck output would
> block it and cost ~0.4 V — but the **TJA1051T/3 needs 4.75 V minimum**, and 5.0 − 0.4 = 4.6 V puts
> the CAN transceiver out of spec on every board, all the time, to protect against a case that is
> merely inconvenient. Trading a permanent violation for an occasional annoyance is a bad trade.
>
> **Accepted, with two consequences that are real work:**
> 1. **Firmware must not trust `+12V_P` as a "vehicle present" signal without a threshold well above
>    4 V.** `power.h` asserts at 7 V and releases at 6 V, and `test_power.c` sweeps the whole
>    3.0–5.5 V back-feed band. Get this wrong and a 500 mA host port is asked for a 450 mA LED cap.
> 2. **Bring-up gate:** plug USB into a `J1`-mated board with the car off, and watch `+12V_P` on a
>    scope. Watch specifically for the buck hiccup-oscillating as its own back-fed `VIN` crosses
>    UVLO — the LMR36015's UVLO threshold has not been read, so this is an observation to make, not
>    a prediction to trust.

> ### ⚠ CAN is not guaranteed on USB power
>
> At `VBUS − V_D6` ≈ 4.7 V the **TJA1051T/3 is below its 4.75 V minimum supply**. Not damaging, not
> guaranteed. This is a consequence of one build rather than two: the transceiver used to be DNP on
> sim boards, so the question never arose. Treat "car cable → CAN, USB cable → HID" as the design,
> not as a limitation discovered later.
>
> If CAN-on-USB is ever wanted, the cheap route is to bring the transceiver's `S` pin (currently
> hard-tied to `GND` for normal mode) to a spare GPIO so firmware can hold it in standby on USB
> power — which also returns its 70 mA to the LED budget. **Not done in Rev B.5**, recorded here so
> the option is not rediscovered from scratch.

#### USB current budget — re-derived for the single build

The old sim-variant number was **350 mA of LEDs**, and it was correct *at the time*: the CAN
transceiver was DNP on those boards. It is fitted on every board now, and 70 mA it never accounted
for is now spent:

| Load on `+5V` | Worst case | Basis |
|---|---|---|
| AP2112K → `+3V3` (MCU + display) | 110 mA | LDO, so 1:1 from the 5 V side |
| TJA1051T/3 transmitting dominant | 70 mA | supply spec, dominant state |
| 74AHCT1G125 LED buffer | <1 mA | logic only |
| **Non-LED total** | **≈180 mA** | |
| USB allowance after enumeration | 500 mA | 5.1 kΩ Rd on both CC pins = default USB power |
| **Headroom for LEDs** | **320 mA** | 500 − 180 |
| **Cap set in firmware** | **300 mA** | 20 mA in hand |

110 + 70 + 350 = **530 mA**, which does not fit. **Collapsing the variants changed this number** —
and it changed it in a build flag that nobody would have thought to revisit. That is the argument
for measuring the supply rather than compiling an assumption about it.

Before enumeration a device may draw only 100 mA, which the non-LED load nearly consumes on its own,
so the firmware cap is **zero until the host has configured us**. LEDs stay dark for the first
moments on a USB cable, deliberately.

> **No CC-current sensing, and that is a decision (defect 8.16).** An earlier version of
> `sim-variant-instructions.md` promised a DNP `R_CC_SNS` footprint for reading the CC pin voltage to
> detect a 1.5 A or 3 A source and lift the cap. **That footprint was never in this file or in either
> BOM** — it was described as "provided" and did not exist. It is not being added: lifting the cap is
> an optimisation with no safety value (300 mA is safe on *any* compliant source), and it would need
> two facts this project has not verified — an ADC channel number for a spare pin from DS12288
> Table 12, and the USB-C Rp advertisement currents from the Type-C spec. Adding a footprint whose
> function rests on two unread documents is how placeholders get built into boards.

---

## 4. Sheet `wheel-can-io.SchDoc`

### 4.1 U3 — **TJA1051T/3**, SO-8 (verified pinout)

| Pin | Name | Connection |
|---|---|---|
| 1 | TXD | `CAN_TX` (PB9) |
| 2 | GND | `GND` |
| 3 | VCC | `+5V` |
| 4 | RXD | `CAN_RX` (PB8) |
| 5 | VIO | `+3V3` |
| 6 | CANL | `CAN_L` |
| 7 | CANH | `CAN_H` |
| 8 | S | `GND` (silent mode off = normal mode) |

`C12` 100 nF 0402 at pin 3; `C13` 100 nF 0402 at pin 5.

### 4.2 Bus protection and termination

| Ref | Part / value | Connection | Fit |
|---|---|---|---|
| `D2` | **PESD2CAN**, SOT-23 — 24 V standoff, V_CL 41 V @ 5 A | across `CAN_H`/`CAN_L` to `GND` | Fit |
| `R_T1` | 60.4 Ω 1 %, 0805 | `CAN_H` → `NET_TERMMID` | **DNP** |
| `R_T2` | 60.4 Ω 1 %, 0805 | `NET_TERMMID` → `CAN_L` | **DNP** |
| `C_T1` | 4.7 nF, 50 V, 0603 | `NET_TERMMID` → `GND` | **DNP** |

Add a bold schematic note: *populate the termination only if this board is a physical end of the bus.*
PESD2CAN's 41 V clamp is well under the TJA1051's ±58 V bus tolerance.

### 4.3 Paddle pass-through and sense

Per line (UP and DOWN):

| Ref | Value | Connection |
|---|---|---|
| `D3` / `D4` | **SMAJ24CA** bidirectional TVS, SMA | `PADDLE_UP` → `GND` / `PADDLE_DOWN` → `GND`, at the connector |
| `R6` / `R7` | **150 kΩ** 1 %, 0402 | `PADDLE_UP` → `PADDLE_UP_SNS` / `PADDLE_DOWN` → `PADDLE_DN_SNS` |
| **`R6b` / `R7b`** | **39 kΩ** 1 %, 0402 | **`PADDLE_UP_SNS` → `GND` / `PADDLE_DN_SNS` → `GND`** — the lower half of the divider. **Without this the pin sits at the full paddle-line voltage** (defect 1.8) |
| `C14` / `C15` | 1 nF, 0402 | each `*_SNS` net → `GND`, at the MCU pin |
| **`D14` / `D15`** | **BAV199** dual low-leakage silicon, SOT-23 | **Use BOTH halves on each net**, exactly as the dash DAQ clamps in `dash-schematic-complete.md` §3: **upper diode** anode → `*_SNS`, cathode → `+3V3`; **lower diode** cathode → `*_SNS`, anode → `GND`. The lower half is not optional — `D3`/`D4` are **bidirectional** SMAJ24CA parts, so a negative transient clamps at −38.9 V and the divider presents **−8.03 V** at the pin, against an absolute minimum of VSS − 0.3 V. **Load-bearing, not belt-and-braces** — see the transient box below (defect 8.7). Same part and same reasoning as the dash DAQ clamps |

> ⚠ **The divider alone does not survive a paddle-line transient (defect 8.7).** `D3`/`D4` are
> SMAJ24CA parts that clamp at **38.9 V**. The divider passes 38.9 × 39/189 = **8.03 V** to the pin —
> **twice the 4.0 V `TT_a` absolute maximum** (DS12288 Table 14). And there is no self-rescue: Table 15
> note 3 states *"positive injection (when V_IN > V_DD) is not possible on these I/Os"*, i.e. these pins
> have **no upper clamp diode to VDD**, so the overvoltage appears across the pin structure instead of
> being shunted. `D14`/`D15` supply the missing clamp, holding the pin near 3.9 V while the 150 kΩ
> upper leg limits the diode current to (38.9 − 3.9)/150 kΩ = **0.23 mA** — trivial for a BAV199.
> BAV199 rather than a Schottky for the same reason as the DAQ front end: leakage into a high-impedance
> node *is* signal error (defect 5.2).

`PADDLE_UP` and `PADDLE_DOWN` run **as copper** from J1 pins 4 and 5 straight out — they are not
switched or buffered by this board. The taps are observation only: 189 kΩ total means **63 µA** of
influence at 12 V, 85 µA at a 16 V charging-system maximum.

> ⚠ **The sense taps are ADC inputs, not GPIO.** PB0/PB1 are `TT_a` pins with a **4.0 V absolute
> maximum**, and the Nexus pulls these lines to 5 V or 12 V depending on configuration. The
> 150 kΩ/39 kΩ divider puts a 16 V line at 3.30 V, a 12 V line at 2.48 V and a 5 V line at 1.03 V — both safe, but 1.03 V is
> below the 2.31 V logic-high threshold, so **no single divider works as a digital input for both
> pull-up voltages.** Read them with the ADC and threshold in firmware; the measured level also tells
> you which pull-up the ECU is actually using. Origin: defect 1.8.

---

## 5. Sheet `wheel-hmi.SchDoc`

### 5.1 The standard conditioning cell — repeat identically 24 times

Make this a device sheet or snippet. For every encoder A, B, SW line and every button line:

```
switch/encoder contact ──┬── R(1 kΩ) ──┬── MCU pin
                         │             ├── C(100 nF) ── GND
                        GND            └── R(10 kΩ) ── +3V3
```

- Series 1 kΩ limits injected current to <5 mA even on a direct short to 5 V.
- 10 kΩ pull-up is **external**, so the line has a defined state while the MCU is in reset.
- Falling-edge τ = 100 µs, rising-edge τ = 1 ms — both comfortably faster than the ~8 ms between
  edges of a briskly spun encoder.

Designators: `R13`–`R36` (series 1 kΩ), `R37`–`R60` (pull-ups 10 kΩ), `C51`–`C74` (100 nF).

### 5.2 Encoders

| Ref | Part | Nets |
|---|---|---|
| `ENC1`–`ENC4` | **PEC09-2120F-S0012** (right-angle, 12 PPR, 12 detent, push switch) | A→`ENC1_A`… B→`ENC1_B`… SW→`ENC1_SW`…, commons → `GND` |
| `ENC5`,`ENC6` | **PEC11H-4120F-S0020** (vertical bushing, 20 PPR, 20 detent, push switch) | `ENC5_A/B/SW`, `ENC6_A/B/SW`, commons → `GND` |

### 5.3 Buttons

| Ref | Part | Nets |
|---|---|---|
| `SW1`–`SW6` | C&K **KSC4** series IP67 tactile | one side → `BTN1`…`BTN6`, other side → `GND` |
| `J5`,`J6` | JST-GH 2-pin `SM02B-GHS-TB` | pin 1 → `BTN5` / `BTN6` (parallel with SW5/SW6), pin 2 → `GND` |
| `D7`,`D8` | **BAV99** dual series diode, SOT-23 | clamp `BTN5`/`BTN6` to `+3V3` and `GND` at J5/J6 (lines leave the board) |

### 5.4 Satellite encoder provision — **all DNP**

| Ref | Part | Nets |
|---|---|---|
| `J7`–`J10` | JST-GH 5-pin `SM05B-GHS-TB` | 1 `+3V3`, 2 `ENCn_A`, 3 `ENCn_B`, 4 `ENCn_SW`, 5 `GND` (n = 1…4) |
| `D9`–`D12` | BAV99, SOT-23 | clamp each satellite line to `+3V3`/`GND` |

Fitting `J7` instead of `ENC1` moves that thumb encoder onto a satellite board with no respin. The
conditioning cell stays on the main board either way.

---

## 6. Sheet `wheel-leds.SchDoc`

| Ref | Part / value | Connection |
|---|---|---|
| `U6` | **74AHCT1G125**, SOT-353 | VCC → `+5V`, GND → `GND`, `/OE` → `GND`, A → `LED_DATA_3V3` (PA6), Y → `NET_LEDBUF` |
| `C_U6` | 100 nF, 0402 | `+5V` → `GND` at U6 |
| `R12` | 300 Ω, 0402 | `NET_LEDBUF` → `LED_DATA_5V` |
| `LED1`–`LED24` | **WS2812B-2020**, LCSC C965555 | VDD → `+5V`, VSS → `GND`, DIN of LED1 ← `LED_DATA_5V`, then DOUT→DIN daisy chain through LED24 |
| `C25`–`C48` | 100 nF, 0402 | one per LED, `+5V` → `GND` at each LED |
| `C49`,`C50` | 100 µF, 10 V | one at the shift bar, one shared by the TC/lockup bars |

Chain order: `LED1…LED16` = shift bar left→right; `LED17…LED20` = TC bar; `LED21…LED24` = lockup bar.

**AHCT is mandatory, not a substitution option.** Its TTL input threshold (V_IH 2.0 V) accepts 3.3 V
logic legally; the WS2812 needs ~0.7 × 5 V = 3.5 V on its data pin, which 3.3 V logic cannot
guarantee directly. A plain HC part would not work.

---

## 7. Sheet `wheel-display.SchDoc` — COLOUR display

`DS1` = **JDI LPM013M126A** — 1.28", **176 × 176, 8 colours**, reflective memory-in-pixel.
`J3` = 10-pin 0.5 mm FPC ZIF.

**Pinout verified against JDI specification Ver.01 — and it is pin-for-pin identical to the Sharp
mono part it replaces**, so this is a BOM change only: same footprint, same nets, same firmware
structure.

| Pin | Signal | Connect to |
|---|---|---|
| 1 | SCLK | `LCD_SCLK` (PA5) |
| 2 | SI | `LCD_SI` (PA7) |
| 3 | SCS | `LCD_SCS` (PA4) |
| 4 | EXTCOMIN | `LCD_EXTCOMIN` (PC3) |
| 5 | DISP | `LCD_DISP` (**PB13**) — H = show memory, L = black, memory retained either way |
| 6 | VDDA | `+3V3` |
| 7 | VDD | `+3V3` |
| 8 | **EXTMODE** | **`+3V3` via `R_EXTMODE` 0 Ω** (JDI: "H = enable EXTCOMIN, connect to VDD"); `R_EXTMODE_L` 0 Ω to `GND` **DNP** |
| 9 | VSS | `GND` |
| 10 | VSSA | `GND` |

`C75` 100 nF + `C76` 1 µF at the connector, from `+3V3` to `GND`.

### Three rules this part imposes — none are optional

1. **VDD and VDDA both come from the MCU's own `+3V3` rail.** The datasheet gives
   **V_IH = VDD − 0.1 V**, so a logic high must be within 100 mV of the display's own supply. Sharing
   the rail with the MCU makes its output high track the display's threshold. Powering the display
   from a separate or lower 3.3 V source breaks this.
2. **VDDA must never exceed VDD** (spec: VDDA max = VDD). Both tie to `+3V3`; do not give VDDA a
   separate higher or filtered rail.
3. **EXTMODE must not float**, exactly as on the Sharp part. High selects the hardware EXTCOMIN path
   used here. Floating leaves COM inversion undefined, which lets DC bias build across the liquid
   crystal and permanently damages the panel.

### Firmware constraints (not visible on the schematic — do not lose these)
- **SCLK maximum is 2.00 MHz** (1.00 MHz typical). The STM32 will happily clock SPI1 far faster;
  cap it in firmware.
- Toggle `LCD_EXTCOMIN` at ~1 Hz continuously whenever the panel is powered.

### Ratings and risks
Power is **115.5 µW max** — irrelevant in the wheel budget. Absolute max VDD 3.6 V.
⚠ **Operating temperature is −20 … +70 °C**, narrower than the rest of the wheel BOM; a black wheel
in direct sun can exceed that at the panel surface (**assumption A9** — measure it on a summer
track day).

**Mono fallback, zero board change:** if colour stock fails, fit the **Sharp LS013B7DH05**
(144 × 168 mono, LCSC `C17500193`, JLC-assemblable). Identical 10-pin pinout. Keep both in the
library. Sourcing for the JDI part is specialty distributors (Switch-Science, Data Modul, Youritech)
rather than Digi-Key/LCSC, which is the one thing that is *worse* about the colour option.

## 8. Connector J1 and the top sheet

`J1` — Deutsch **DTM04-6P** (panel) / DTM06-6S (harness), via pigtail:

| Pin | Net |
|---|---|
| 1 | `+12V_IN` |
| 2 | `CAN_H` |
| 3 | `CAN_L` |
| 4 | `PADDLE_UP` |
| 5 | `PADDLE_DOWN` |
| 6 | `GND` |

Top sheet: place the seven sheet symbols, wire the ports, then **Project ▸ Validate**. Zero errors and
zero unsuppressed warnings before moving to layout.

---

## 9. Complete MCU pin assignment (verified against STM32G474 datasheet Table 13)

| Pin | Net | Peripheral justification |
|---|---|---|
| PC0 / PC1 | `ENC1_A` / `ENC1_B` | TIM1_CH1 / TIM1_CH2 |
| PC6 / PC7 | `ENC2_A` / `ENC2_B` | TIM3_CH1 / TIM3_CH2 |
| PB6 / PB7 | `ENC3_A` / `ENC3_B` | TIM4_CH1 / TIM4_CH2. ⚠ **PB6 is also `UCPD1_CC1`** — firmware *must* set `PWR_CR3.UCPD1_DBDIS`, or a 5.1 kΩ dead-battery pull-down kills this input (defect 8.2 below) |
| PA15 / PB3 | `ENC4_A` / `ENC4_B` | TIM2_CH1 / TIM2_CH2 |
| PB2 / PC2 | `ENC5_A` / `ENC5_B` | **TIM20_CH1 / TIM20_CH2** — TIM20 is an advanced-control timer and *does* implement encoder mode. (Was PB14/PB15 on TIM15, which has channels but **no quadrature decoder** — defect 1.5.) |
| PA0 / PC12 | `ENC6_A` / `ENC6_B` | TIM5_CH1 / TIM5_CH2 |
| PC4, PC5, PC8, PC9, PC10, PC11 | `ENC1_SW`…`ENC6_SW` | GPIO input |
| PC13, PD2, PA3, PB10, PB11, PB12 | `BTN1`…`BTN6` | GPIO input |
| PA6 | `LED_DATA_3V3` | TIM16_CH1 + DMA |
| PA5 / PA7 / PA4 | `LCD_SCLK` / `LCD_SI` / `LCD_SCS` | SPI1_SCK / SPI1_MOSI / GPIO |
| PB13 / PC3 | `LCD_DISP` / `LCD_EXTCOMIN` | GPIO (EXTCOMIN is a ~1 Hz software toggle). **`LCD_DISP` moved off PC2**, which is now `ENC5_B` on TIM20_CH2 (defect 1.5) |
| PB8 / PB9 | `CAN_RX` / `CAN_TX` | FDCAN1 — only option once USB claims PA11/PA12 |
| PA11 / PA12 | `USB_DM` / `USB_DP` | USB FS |
| PA13 / PA14 | `SWDIO` / `SWCLK` | debug |
| PA9 / PA10 | `DBG_TX` / `DBG_RX` | USART1. ⚠ **Also `UCPD1_DBCC1` / `UCPD1_DBCC2`** — a high level here arms the dead-battery pull-down on PB6 / PB4. See defect 8.2 |
| PA1 / PA2 | `V12_SENSE` / `V5_SENSE` | **ADC12_IN2 / ADC1_IN3** — channel *numbers*, verified in DS12288 Table 12. PA0 is IN1, so the sequence PA0→PA3 is IN1, IN2, IN3, IN4; it is **not** indexed from the pin number (defect 8.3) |
| PB0 / PB1 | `PADDLE_UP_SNS` / `PADDLE_DN_SNS` | **ADC1_IN15 / ADC1_IN12 — read as ADC, never as GPIO** (defect 1.8). `TT_a` pins: 4.0 V absolute max, and a BAV199 clamp to `+3V3` is **required** (defect 8.7) |
| **PF0 / PF1** | `NET_OSC_IN` / `NET_OSC_OUT` | **HSE crystal — LQFP-64 pins 5 and 6.** Mandatory for 1 Mbit CAN (§3.2) |
| PA8, PB4, PB5, **PB14, PB15** | spare — PB14/PB15 freed when ENC5 moved off TIM15 | bring to test points if convenient |

**Every encoder pair is CH1+CH2 of one timer, *and that timer supports encoder mode*.** Both halves
matter. Four of six pairs were wrong in the first draft for failing the first test, and a fifth was
wrong for failing the second — TIM15 has two channels but no quadrature decoder. The timers that
**do** support encoder mode on this part are **TIM1, TIM2, TIM3, TIM4, TIM5, TIM8, TIM20** (and
LPTIM1). TIM15/16/17 do **not**. Do not substitute encoder pins without checking both properties.

### ⚠ 9.1 The UCPD dead-battery trap (defect 8.2) — a firmware requirement, not a layout one

This board does **not** use the USB Type-C Power Delivery peripheral. That does not matter: the
dead-battery pull-downs are armed by *pin voltage*, not by enabling UCPD. DS12288 Table 12, note 6:

> "After reset, a pull-down resistor (Rd = 5.1 kΩ from UCPD peripheral) can be activated on PB6, PB4
> (UCPD1_CC1, UCPD1_CC2). The pull-down on PB6 (UCPD1_CC1) is activated by high level on PA9
> (UCPD1_DBCC1). The pull-down on PB4 (UCPD1_CC2) is activated by high level on PA10 (UCPD1_DBCC2).
> This pull-down control … can be disabled by setting bit UCPD1_DBDIS = 1 in the PWR_CR3 register."

`PA9` is `DBG_TX` (USART1_TX), and **an idle UART line sits high**. So the instant the debug UART is
initialised, the MCU arms a 5.1 kΩ pull-down on its own `PB6` = `ENC3_A`. Against that pin's 10 kΩ
conditioning pull-up the idle level becomes 3.3 × 5.1 / 15.1 = **1.11 V**, below the 2.31 V `FT_c`
V_IH (Table 54, 0.7 × VDD). **ENC3_A can then never read high and TIM4 quadrature decode stops.**

The failure mode is deliberately cruel: encoder 3 works in a production build with the debug UART
off, and breaks the moment you attach a debug cable to find out why — it fails only while observed.

**Required in firmware on both boards, before any GPIO setup:**

```c
RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
PWR->CR3      |= PWR_CR3_UCPD1_DBDIS;   /* release Rd on PB4/PB6 — DS12288 Table 12 note 6 */
```

Treat this as provisioning-class, exactly like `nSWBOOT0`. **There is no hardware fix**: every
encoder-capable timer pair on LQFP-64 is already allocated, so `ENC3` cannot move, and shrinking the
10 kΩ pull-up to beat the 5.1 kΩ Rd lands on V_IH with no margin while breaking the low level
against the 1 kΩ series resistor. The firmware bit is the fix; this box is what keeps it.

**Bring-up gate:** with a debug adapter attached, confirm ENC3 counts in both directions.

---

## 10. Remaining open items

| Item | Why it is open | Closes by |
|---|---|---|
| LMR36015 variant fSW | **CLOSED — order `LMR36015FBRNXR` (1 MHz, FPWM).** ⚠ The part **LCSC stocks** is `LMR36015AQRNXRQ1`, which is **400 kHz, non-FPWM** — if you buy that one, `L1 = 15 µH` and `C_OUT = 3 × 22 µF`. An earlier version of this row said "stocked variants are 1 MHz", which is backwards and contradicted §2.2 in the same file (defect 8.12) | Reading the ordering table for the part you actually buy |
| FPC contact side (top vs bottom) | Not stated in the extracted spec; a top-contact connector mirrors the pinout | Check the mechanical drawing before selecting `J3` |
| HSE crystal part + load caps | Spec derived (≤50 ppm, CL 8–12 pF); exact part not chosen | Pick a part, then set `C_X1/2 = 2 × (CL − C_stray)` from **its** datasheet |
| WS2812B-2020 exact current | Datasheet is image-only, no text layer | Bench measurement (assumption A3) — the 0.45 A firmware cap holds regardless |
| SMAJ24CA / SMBJ5.0A parameters | Not read; low risk since standoff clearly exceeds their rails | Quick datasheet check at G6 |

Nothing in this list blocks starting the schematic. All four are closable before the G6 pre-order gate.
