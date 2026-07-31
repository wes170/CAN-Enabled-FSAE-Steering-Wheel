# Altium Instructions — Steering Wheel PCB (`FSAE-WHEEL`)

> **Permanent memory file.** Follow in order. Companion files: `hardware-selections.md` (what & why),
> `system-architecture-and-can.md` (nets, budgets, protocol), `sim-variant-instructions.md` (sim-rig use — **no longer a variant**),
> `engineering-rigor.md` (gates you must pass before ordering).
> Written for Altium Designer 23+; menu paths noted where they matter.

## 0. Fixed design facts (do not drift)

- **Connector contract (DTM04-6P, panel-mounted, pigtail to PCB J1):**

| Pin | Net | Notes |
|---|---|---|
| 1 | `+12V_IN` | **vehicle 12V from harness** (Rev B — was 5V from dash in Rev A) |
| 2 | `CAN_H` | 1 Mbit/s Haltech bus |
| 3 | `CAN_L` | |
| 4 | `PADDLE_UP` | passthrough to Nexus DI |
| 5 | `PADDLE_DOWN` | passthrough to Nexus DI |
| 6 | `GND` | |

> **Rev B change:** the wheel is now 12V-fed and standalone — it no longer requires the dash board to
> operate. Rationale and the contact-resistance arithmetic: `hardware-selections.md` §0.2 and
> `system-architecture-and-can.md` §3.

- **MCU pin map (STM32G474RET6) — ⚠ SUPERSEDED — use the schematic-definition file ⚠**

> **Superseded — do not capture from this table.** The verified pin map now lives in
> `memory/wheel-schematic-complete.md §9`, rebuilt against STM32G474 datasheet Table 13. The table below is kept only
> as revision history. (`datasheet-verification.md` §1 and §6.)

| MCU pin | Net | Function |
|---|---|---|
| PA0 | `V12_SENSE` | ADC, 12V input monitor (47k/10k divider) |
| PA1 | `V5_SENSE` | ADC, local 5V rail monitor (10k/10k divider) |
| PA4 / PA5 / PA7 | `LCD_SCS` / `LCD_SCLK` / `LCD_SI` | Sharp memory LCD (SCS is **active-high**) |
| PB3 / PB6 | `LCD_DISP` / `LCD_EXTCOMIN` | DISP enable; EXTCOMIN toggled ~1 Hz (TIM4_CH1) |
| PA8 | `LED_DATA_3V3` | TIM1_CH1 + DMA → 74AHCT1G125 → `LED_DATA_5V` |
| PA9 / PA10 | `DBG_TX` / `DBG_RX` | USART1 debug header (JST-GH 3-pin) |
| PA11 / PA12 | `USB_DM` / `USB_DP` | USB-C (sim/DFU) |
| PA13 / PA14 | SWD | Tag-Connect TC2030 |
| PB8 / PB9 | `CAN_RX` / `CAN_TX` | FDCAN1 → TJA1051T/3 |
| PB4 / PB5 | `PADDLE_UP_SNS` / `PADDLE_DN_SNS` | 100k tap off paddle lines |
| PC0/PC1, PC2/PC3, PC4/PC5, PC6/PC7, PB0/PB1, PB2/PB10 | `ENC1_A/B` … `ENC6_A/B` | 4 thumb (**PEC09**, right-angle) + 2 faceplate (PEC11H). Pin map is unchanged if thumb encoders move to satellite boards — the nets just leave via JST-GH instead of local pads |
| PB11–PB15, PC13 | `ENC1_SW` … `ENC6_SW` | encoder push switches |
| PC8–PC12, PD2 | `BTN1` … `BTN6` | sealed tactiles / JST-GH remotes |
| BOOT0 | **no strap — option bit only** | `PB8-BOOT0` is `FDCAN1_RX`; see §3.2 item 2 |

- Stackup: **4-layer JLC7628**: L1 signal, L2 solid GND, L3 3V3/5V pours + slow signals, L4 signal.
- LED chain order: `LED1…LED16` = shift bar left→right, `LED17…LED20` = TC bar, `LED21…LED24` = lockup bar.

## 1. Project setup

1. **File ▸ New ▸ Project** → template *Default*, name `FSAE-WHEEL.PrjPcb`, save under `hardware/wheel/` in this repo. Enable **version control (Git)** in project options; commit the empty project.
2. Add documents: `wheel-power.SchDoc`, `wheel-mcu.SchDoc`, `wheel-can-io.SchDoc`, `wheel-hmi.SchDoc`, `wheel-leds.SchDoc`, `wheel-display.SchDoc`, top sheet `wheel-top.SchDoc`, and `FSAE-WHEEL.PcbDoc`.
3. **Project ▸ Project Options ▸ Error Reporting**: set *Nets with only one pin*, *Floating power object*, *Duplicate part designators* to **Fatal Error**. Comparator: leave all differences enabled. These are the ERC gates in engineering-rigor.md.
4. Parameters (Project ▸ Parameters): `Rev = B`, `Board = FSAE-WHEEL`, add title-block template referencing them.

## 2. Libraries

1. Create `FSAE-Common.SchLib` / `FSAE-Common.PcbLib` inside the project (shared later with the dash — keep them in `hardware/lib/`).
2. Source symbols/footprints in this priority order, **verifying every footprint against the datasheet mechanical drawing** (rigor rule — library errors are the #1 cause of respins):
   - Altium **Manufacturer Part Search** panel (place directly, then *right-click ▸ Add to library*),
   - SnapEDA / Ultra Librarian import (run **IPC-compliance check**: Reports ▸ Footprint comparison),
   - IPC Footprint Wizard (Tools menu in PcbLib) for anything missing — use datasheet nominal dims, density level **N**.
3. Mandatory footprint checks (print datasheet page, tick off): STM32G474RET6 LQFP-64 (0.5 mm pitch — verify pad 0.28×1.5 mm class), TJA1051 SO-8, **PEC09 right-angle** (THT — verify the body sits flat against the board and the shaft exits *parallel* to the PCB at the intended edge; check shaft length 15/20/25 mm against the faceplate depth before committing), PEC11H (bushing hole Ø9.5 mm + anti-rotation slot **on the faceplate drawing too**), WS2812B-2020 (2.2×2.0 mm, pin-1 dot orientation), Sharp LCD 10-pin FPC 0.5 mm bottom-contact ZIF (contacts flip if you pick top-contact — check twice), USB-C 16-pin, KSC4 tactiles, JST-GH horizontal, TC2030 footprint (no part, copper+3 locating holes), **LMR36015 VQFN-HR-12 (RNX)** — 12 pads incl. the large SW/PGND thermal pads; verify against the datasheet package drawing, *not* a generic QFN-12 (defect 5.1 replaced the AP63205 TSOT-26 here), plus its inductor. AP2112K SOT-25.
4. For every JLC-assembled part, add parameters `LCSC = Cxxxxxx` and `JLC-Rotation` (fill after §7 check). Key numbers already verified: MCU `C521608`, LEDs `C965555`.

## 3. Schematic capture — ⚠ SUPERSEDED BY THE SCHEMATIC-DEFINITION FILE ⚠

> **Capture from `memory/wheel-schematic-complete.md` instead.** That file is the authoritative, datasheet-verified
> component-and-net definition; the sheet-by-sheet notes below are earlier working text and still
> name parts that have since been **rejected** (AP63205, AMS1117, BAT54S) and values that have
> changed.
>
> **This file remains authoritative for *process*** — project setup (§1), libraries (§2), layout
> (§4), DRC and gates (§5), why there are **no variants** (§6) and manufacturing outputs (§7). Use it for how to work;
> use the schematic-definition file for what to draw.
>
> *(Split introduced 2026-07 after an audit found stale values being repeated downstream — lesson L21.)*

### 3-legacy. Earlier sheet notes (historical)

Net naming: exactly the names in §0. Use ports between sheets; no hidden power-net magic except `GND`.

### 3.1 `wheel-power.SchDoc` — **Rev B: 12V automotive entry**
This is now the *same input stage as the dash* (§1.1 of the dash doc). Copy that sheet, don't redraw it.
1. `+12V_IN` from J1 pin 1 → **1 A polyfuse** → reverse-polarity **P-FET** (DMP3056L class: source→board,
   gate→GND via 10k, gate zener 12V) → net `+12V_P`.
2. At the J1 side: **SMBJ33A** to GND + 100 nF. (Scope the clamp during the A8 transient test; drop to
   SMBJ26A if the measured clamp threatens the buck's abs-max.)
3. **LMR36015FBRNXR** sync buck `+12V_P` → net `+5V`, 1 MHz, VQFN-HR-12 (RNX). ***Not* the AP63205** — its 35 V absolute maximum cannot face a vehicle battery behind a TVS that clamps at 53.3 V (defect 5.1, the most expensive defect in this project). Full pinout and every passive value: `wheel-schematic-complete.md` §2.2. Summary: L1 = 10 µH (I_sat ≥ 2 A), C_IN = 4.7 µF 50 V **+ 2 × 220 nF 50 V, one at each VIN–PGND pair** (the datasheet says these *must* be fitted), C_OUT = 3 × 15 µF 16 V, C_BOOT = 100 nF, C_VCC = 1 µF, R_FBT = 100 kΩ, R_FBB = 24.9 kΩ, C_FF = 20 pF. Tie NC (pin 3) to `NET_SW`; EN straight to `+12V_P`. This rail feeds the LED bars and the CAN transceiver.
4. **AP2112K-3.3TRG1** LDO (SOT-25) from `+5V`: 1 µF in / 1 µF + 100 nF out, net `+3V3`. ***Not* the AMS1117** — that part requires a *tantalum* output capacitor for stability and is unstable on the ceramics specified here (defect 5.4). The AP2112K is explicitly ceramic-stable.
5. `+3V3` → ferrite bead (600Ω@100MHz) → `+3V3A` (VDDA/VREF+) with 1 µF + 100 nF.
6. Dividers: 47k/10k from `+12V_P` → `V12_SENSE`; 10k/10k from `+5V` → `V5_SENSE`; 100 nF at each ADC pin.
7. Test points: `+12V_P`, `+5V`, `+3V3`, `GND` ×2 (loop for scope ground spring).
8. USB VBUS OR-ing: **BAT60A Schottky from VBUS into the `+5V` rail (downstream of the buck)** through
   the `R_VBUS` link, **fitted on every board since Rev B.5** — see `sim-variant-instructions.md`.
   Never OR into `+12V_P`. ⚠ Fitting it also creates the back-feed path of defect 8.15: USB reaches
   `+12V_P` at ~4 V through the buck's high-side body diode. Accepted, not fixed — read
   `wheel-schematic-complete.md` §3.4a before deciding to "improve" this.

### 3.2 `wheel-mcu.SchDoc`
1. STM32G474RET6: 100 nF at **every** VDD pin + one 4.7 µF bulk; VDDA from `+3V3A`.
2. NRST: 100 nF to GND (+ pin to TC2030).
   **BOOT0: DO NOT fit a pulldown or strap.** `PB8-BOOT0` is also `FDCAN1_RX` on this package, and a
   pulldown would fight the transceiver's RXD output. BOOT0 must come from the option bytes: **`nSWBOOT0` (FLASH_OPTR[26]) = 0** and
   **`nBOOT0` (FLASH_OPTR[27]) = 1**, set with **STM32CubeProgrammer** and re-checked after any mass erase — otherwise an idle
   (recessive-high) CAN bus makes the MCU boot into the system bootloader every power-on. Full
   analysis: `memory/datasheet-verification.md` §1 defect 1.3. DFU entry is via USB DFU or an
   SWD-triggered jump, never a BOOT0 strap.
3. Tag-Connect TC2030-CTX wired SWDIO/SWCLK/NRST/3V3/GND.
4. USB-C: VBUS → Schottky (BAT60A) OR-ing into `+5V` **through the 0 Ω link `R_VBUS`, FITTED on every board** (Rev B.5 — one build, two cables; see sim doc §2), CC1/CC2 → 5.1 kΩ to GND, D+/D− → **USBLC6-2SC6** → PA11/PA12. Shield to GND via 1 MΩ ∥ 4.7 nF.
5. Debug UART on JST-GH 3-pin (TX, RX, GND).

### 3.3 `wheel-can-io.SchDoc`
1. **TJA1051T/3**: VCC=`+5V` (100 nF), VIO=`+3V3` (100 nF), TXD←PB9, RXD→PB8, S → GND.
2. CANH/CANL → **PESD2CAN** to GND; DNP split termination: `R_T1`,`R_T2` = 60.4Ω 1% in series CANH→CANL, midpoint → 4.7 nF → GND. Mark all three **DNP** with a bold schematic note: *populate only if wheel is the physical end of bus*.
3. Paddles: J1.4 → `PADDLE_UP` → **SMAJ24CA** to GND (at connector). Sense tap is a **divider, not a single resistor**: `R6` **150 kΩ** from `PADDLE_UP` → `PADDLE_UP_SNS`, `R6b` **39 kΩ** from `PADDLE_UP_SNS` → GND, `C14` 1 nF to GND at the MCU pin, and `D14` **BAV199** from `PADDLE_UP_SNS` → `+3V3`. Mirror for DOWN (`R7`/`R7b`/`C15`/`D15`).
   - **Do not fit the 100 kΩ single-resistor tap shown in earlier revisions of this file** — with no lower leg the pin sits at the full paddle-line voltage (defect 1.8), and without `D14`/`D15` a TVS clamp event puts 8 V on a 4.0 V `TT_a` pin (defect 8.7). Both are required.
   - These are **ADC inputs, not GPIO** — no single divider gives a valid logic high for both the 5 V and 12 V ECU pull-up options, so threshold in firmware. Full derivation in `wheel-schematic-complete.md` §4.3.
   - The through-path is just copper — the wheel works with the MCU dead (requirement).

### 3.4 `wheel-hmi.SchDoc`
Repeat this conditioning cell for **every** encoder A/B/SW and button line (make it a snippet/device sheet so it's identical 24 times):
`switch contact → 1 kΩ series → MCU net`, with `10 kΩ pull-up to +3V3` and `100 nF to GND` on the MCU side of the resistor. Switch commons to GND.
- 4 × **PEC09** right-angle (A/B/SW), 2 × PEC11H (A/B/SW).
- **Satellite provision (cheap insurance, do it):** give each thumb encoder a **DNP JST-GH 5-pin
  footprint** wired to the same `ENCn_A/B/SW` + 3V3 + GND nets, placed beside its on-board footprint.
  Populating one and not the other decides at *assembly* time whether that encoder is on-board or on a
  satellite board — no respin either way. Put the 1 kΩ/100 nF/10 kΩ conditioning cell on the main
  board in both cases, and add the **BAV99** clamp on any line routed to a satellite connector
  (it leaves the PCB). Rationale: `hardware-selections.md` §4.1b.
- 6 × KSC4 tactile **plus** 2 × JST-GH 2-pin aux button connectors wired in parallel with BTN5/BTN6 nets, each aux line adding **BAV99** clamp to +3V3/GND at the connector (lines leave the board).

### 3.5 `wheel-leds.SchDoc`
1. **74AHCT1G125**: VCC=`+5V`, A=`LED_DATA_3V3`, OE→GND, Y→ 300Ω → `LED_DATA_5V`.
2. 24 × WS2812B-2020 daisy-chain (DOUT→DIN), each with 100 nF; 2 × 100 µF bulk (one at each bar group). Power from `+5V`.

### 3.6 `wheel-display.SchDoc`
**Verified against the Sharp LS013B7DH05 spec sheet (LD-27503A) §4, table 4-1.** 10-pin FPC:

| Pin | Signal | Connect to |
|---|---|---|
| 1 | SCLK | `LCD_SCLK` (PA5) |
| 2 | SI | `LCD_SI` (PA7) |
| 3 | SCS | `LCD_SCS` (PA4) — chip select |
| 4 | EXTCOMIN | `LCD_EXTCOMIN` (PB6, TIM4_CH1) — external COM inversion input, **H = enable** |
| 5 | DISP | `LCD_DISP` (PB3) — display on/off (Hi = show memory, Lo = white; memory retained either way) |
| 6 | VDDA | `+3V3` (analog supply) |
| 7 | VDD | `+3V3` (digital supply) |
| 8 | **EXTMODE** | **strap — see below. Do not leave floating.** |
| 9 | VSS | GND (digital) |
| 10 | VSSA | GND (analog) |

> **EXTMODE was missing from the previous revision of this instruction and is not optional.**
> It selects *how* COM inversion happens: `EXTMODE = Hi` enables the hardware `EXTCOMIN` pin,
> `EXTMODE = Lo` uses the software serial input flag instead. A memory LCD **must** have COM
> inversion running — it is what prevents a DC bias accumulating across the liquid crystal. Leave
> pin 8 floating and the inversion mode is undefined, which risks permanent image sticking and panel
> degradation. **Strap EXTMODE to `+3V3` via a 0 Ω link (with a DNP pulldown alternative)** so the
> hardware EXTCOMIN path this design uses is actually selected, and firmware toggling PB6 at ~1 Hz
> does what it is supposed to do. Origin: lesson L16.
>
> Still unverified on this part: FPC contact side (top vs bottom) and SCS polarity/timing. Confirm
> both from the spec sheet before the footprint and firmware are frozen.

Decoupling: 100 nF + 1 µF at the connector, per the spec's note that VDD–GND and VDDA–GND impedance
must be kept low in use.

### 3.7 Top sheet + ERC
Wire sheet symbols, then **Project ▸ Validate**. Zero errors, zero *unsuppressed* warnings — suppressions require a written justification comment (rigor gate G1).

## 4. PCB layout

1. **Board shape:** import the wheel chassis/faceplate DXF (**File ▸ Import ▸ DXF**) onto a mech layer; draw outline; place mounting holes (M3, plated, GND-stitched) per the mechanical design. Keep the quick-release hub keep-out (no parts within the hub boss circle + 2 mm).
2. **Layer stack manager:** 4-layer, pick **JLC7628** preset; L2 = uninterrupted GND (rule: no routing on L2, period).
2b. **Buck placement (new in Rev B):** put the LMR36015 hot loop in the J1 connector corner, loop area
   <20 mm², inductor and input caps tight, output ferrite before the LED bulk caps. Keep it outside the
   quick-release hub keep-out and ≥10 mm from the memory-LCD FPC and encoder conditioning cells. The
   800 kHz WS2812 data line remains the board's worst aggressor — do not route it over the buck.
3. **Placement zones** (component side facing away from driver except HMI):
   - Encoders/buttons/display on **front** side per cockpit ergonomics (display top-center, thumb encoders at grip height L/R, faceplate encoders lower center).
   - LED bars on front, top edge: 16-LED arc; TC bar left, lockup bar right (driver-mnemonic: left = traction).
   - MCU central back; CAN transceiver + TVS **adjacent to J1 pigtail entry**; polyfuse → P-FET → SMBJ33A → buck in that physical order at power entry, no crossing nets. Protection parts always closest to the connector — energy must be clamped before it travels.
4. **Routing rules (Design ▸ Rules):**
   - Clearance 0.2 mm global (**0.4 mm minimum around `+12V_P` and the buck switch node** — higher voltage, and it keeps creepage sane on a board that sees sweat); track 0.25 mm signal / 0.5 mm `+3V3` / 1.0 mm min `+5V` trunk (2 A capable at 35 µm, ΔT<10 °C — widen where space allows); `+12V_P` 0.5 mm is ample at 0.3 A.
   - Via 0.3/0.6 mm. Teardrops on.
   - Diff pairs: `CAN_H/L` and `USB_DP/DM` as pairs. USB: 90Ω differential — get exact geometry from the **JLCPCB impedance calculator** for JLC7628 (≈0.3 mm/0.2 mm class) and enter into the rule; length-match ±0.15 mm. CAN at 1 Mbit/s on a ≤100 mm board is not impedance-critical — route as a coupled pair, keep off noisy zones.
   - `LED_DATA_5V`: single 800 kHz edge-heavy line — route over solid GND, no layer hops if avoidable.
5. **Order of routing:** power trunks → CAN/USB pairs → display/encoder buses → LED chain (short DOUT→DIN hops along the bars) → cleanup. Every signal referenced to L2 GND; if a signal must change layers, add a GND stitching via within 2 mm.
6. **Analog/quiet:** keep `V5_SENSE`/`V12_SENSE`/VDDA parts away from LED bars and the buck; ferrite + VDDA caps within 5 mm of pin 13 (VDDA).
7. **Copper pours:** GND on L1/L4 stitched at ≤5 mm grid near LEDs (thermal spreading — 24 LEDs ≈ 4 W worst case over the bar area).
8. **Silkscreen:** J1 pinout table printed on the back silk (**note the 12V on pin 1**); LED1/LED17/LED21 index marks; polarity/pin-1 everywhere; board name + rev + date.

## 5. DRC & pre-release gates

Run **Tools ▸ Design Rule Check**: zero errors. Then the rigor-file gates: G2 (3D collision check against faceplate STEP — Altium 3D body import), G3 (print 1:1, physically place encoders/hands on paper), G4 (netlist re-verify of §0 tables), G5 (peer review sign-off).

## 6. Variants — **there are none (Rev B.5)**

**Do not create assembly variants on this project.** The `CAR`/`SIM` pair has been deleted: every
part is fitted on every board, and a car wheel and a sim wheel differ only in which cable is plugged
in. `R_VBUS` is fitted. Rationale and the two electrical consequences are in
`sim-variant-instructions.md` §1–2.

`[No Variations]` is therefore the correct selection in every output — it used to be the classic
upload mistake, and now it is the only right answer. If you find a `CAR` or `SIM` variant in the
project, it is left over; delete it rather than maintaining it.

**Still DNP, and these are options rather than variants:** `R_T1`/`R_T2`/`C_T1` (CAN termination —
an installation decision, per car), `J7`–`J10`/`D9`–`D12` (satellite encoder provision),
`R_EXTMODE_L`. An option is a footprint you may not use; a variant forks the BOM.

## 7. Manufacturing outputs (JLCPCB PCBA)

1. **Fabrication:** File ▸ Fabrication Outputs ▸ **Gerber Files** (RS-274X, 4:6, all used layers + outline) and **NC Drill** (4:6, separate PTH/NPTH). Or use an OutJob (`FSAE-WHEEL.OutJob`) capturing all of this — preferred, commit it.
2. **Assembly:** BOM (Reports ▸ Bill of Materials) with columns *Designator, Comment, Footprint, LCSC*; Pick-and-place: File ▸ Assembly Outputs ▸ Generates pick and place (CSV, mm, both sides).
3. Zip gerbers+drill → JLCPCB order: 4-layer, **JLC7628 impedance control**, 1.6 mm, ENIG (flat pads for the 0.5 mm-pitch parts and FPC contacts), black solder mask (glare), remove order number or "specify location" under a bar.
4. PCBA (Economic ok if all parts qualify, else Standard): upload BOM+CPL, then **fix rotations in the JLC preview** against your 3D view — record every corrected rotation in the `JLC-Rotation` parameter so next order is clean (rigor lesson L3).
5. Order **≥3 assembled + 2 bare** (bring-up sacrifices happen). Hand-solder parts (display FPC, DTM pigtail, encoders if out of stock, USB-C if hand-picked): reflow/iron per `engineering-rigor.md` bring-up order.

## 8. Bring-up hook

Do not ship straight to the car: follow the staged bring-up in `engineering-rigor.md` §4 (power-only at 12 V → SWD → CAN loopback → NSP detection with bench Nexus/sniffer → HMI → LEDs → display → in-car).
