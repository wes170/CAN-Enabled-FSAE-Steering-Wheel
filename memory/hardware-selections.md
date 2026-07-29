# Hardware Selections — FSAE CAN Steering Wheel, Dash, and Sim Variant

> **Permanent memory file.** Every part here was chosen from first principles. If a part is ever
> substituted, the substitution must satisfy the same *reasoning*, not just the same footprint.
> Availability was checked July 2026 (LCSC/JLCPCB for PCBA-priority parts, Digi-Key/Mouser for the rest).

## 0. Selection principles (read first)

1. **The wheel sees exactly 5 electrical functions at its connector: +5V, GND, PADDLE_UP, PADDLE_DOWN, CAN (H+L).**
   Every wheel part must live within that constraint — no 12V parts on the wheel, no extra signal wires.
2. **First-principles power reasoning:** the Nexus R5's internal 5V sensor supplies are sized for
   sensors (hundreds of mA, shared with actual sensors). A wheel with ~24 RGB LEDs can transiently
   draw ~0.7–1.0A. **Therefore the wheel's 5V must come from a dedicated 5V source — the dash PCB
   provides a protected 5V/1.5A auxiliary output for this.** Never wire the wheel's 5V to an ECU
   sensor-supply pin.
3. **Sunlight is the display spec, not resolution.** A reflective display gets *brighter* in sunlight;
   an emissive display must fight the sun (~10,000–100,000 lux). So the wheel uses a reflective
   memory-in-pixel LCD and the dash uses a 1000 cd/m² transmissive panel (the practical minimum for
   direct-sun legibility of a color TFT).
4. **Every external line gets conditioning** (series impedance + clamping + filtering) because the
   wheel/dash live on a harness in a vibrating, ESD-prone, alternator-transient environment.
5. **Assembly:** JLCPCB PCBA-available parts preferred; hand-solderable fallback (≥0603, exposed leads)
   where PCBA stock is uncertain. No BGA anywhere.
6. **One MCU platform for all three products** (wheel, dash, sim wheel) → one firmware codebase, one
   debug toolchain, shared schematic blocks.

## 1. MCU — STM32G474RET6 (LQFP-64)

**Selected:** STMicroelectronics **STM32G474RET6**, LQFP-64, 170 MHz Cortex-M4F, 512 KB flash, 128 KB RAM.
LCSC **C521608** (~$3.93, in stock, JLCPCB-assemblable).

**First-principles justification:**
- **Built-in FDCAN controller.** The entire product is a CAN device; an on-die bus controller removes
  an SPI CAN chip (MCP2515/2518), its crystal, its interrupt latency, and a whole class of firmware
  bugs. FDCAN runs classic CAN 2.0 at the Haltech bus rate (1 Mbit/s) natively.
- **Built-in USB FS device** → the sim-rig variant becomes a firmware image + assembly variant, not a
  new board. Also gives DFU bootloader (field reflash with no programmer).
- **5V-tolerant (FT) I/O pins** for the level-shift-free interfaces where needed, 12-bit ADCs with
  4 Msps (dash DAQ), timers with encoder mode for every encoder.
- **Temperature:** -40…+85°C standard grade covers cockpit environment.
- **LQFP-64 over LQFP-48:** the wheel needs ~40 signal pins (6 encoders × 3, 6+ buttons, display,
  LEDs, CAN, USB, paddle sense, debug). 64 pins gives margin without going to unsolderable packages;
  LQFP is hand-reworkable (mixed-assembly requirement).

**Alternatives considered:** STM32G474CET6 (LQFP-48) — $8.83 at LCSC, *more* expensive than the RET6
and pin-starved: rejected. STM32F405 — older, no FDCAN (bxCAN fine, but G4 is cheaper and current).
RP2350/ESP32/Teensy — rejected in trade study (no CAN controller / thermal & determinism / cost & module dependency).

## 2. CAN transceiver — NXP TJA1051T/3

**Selected:** **TJA1051T/3,118** (SO-8): 5V-supplied high-speed CAN transceiver with **VIO pin at 3.3V**.
Widely stocked (LCSC/JLC and Digi-Key). Hand-solderable SO-8 fallback if PCBA stock dips; drop-in
domestic equivalent SIT1051T/3 exists at LCSC.

**First-principles justification:**
- **5V supply → full CAN differential drive** (better noise margin on a car harness than 3.3V-only
  transceivers like TCAN332), while **VIO=3.3V** interfaces directly with the G474 with no level shifting.
- We already have 5V on both boards by requirement, so the 5V supply costs nothing.
- 1 Mbit/s (Haltech bus rate) is well within its 5 Mbit/s rating; ±58V bus fault tolerance survives
  harness mis-wiring to battery.
- `/3` silent-mode pin tied off; no standby logic to get wrong.

**Bus conditioning (both boards):** PESD2CAN (or NUP2105L) dual-line TVS on CANH/CANL, optional
split termination footprint (2×60.4Ω + 4.7nF to GND, **DNP by default** — see architecture doc for
termination policy), common-mode choke footprint (DNP unless emissions testing demands it).

## 3. Power

### 3.1 Wheel (5V in → 3.3V logic)

| Function | Part | Why |
|---|---|---|
| Input protection | **SMBJ5.0A** TVS + **MF-MSMF110** 1.1A polyfuse + reverse-blocking not used (see note) | TVS clamps harness transients/ESD on the 5V line; polyfuse protects the coiled cord/harness from a board fault. Reverse-polarity series diode is **not** used because a 0.3–0.5V drop is 6–10% of a 5V rail; instead the connector (keyed DTM) physically prevents reversal, and the TVS shorts a reversed supply → polyfuse trips. This is a deliberate first-principles trade: keyed connector + fuse beats wasting rail margin. |
| 3.3V rail | **AMS1117-3.3** (SOT-223) | JLCPCB *basic* part (no setup fee, always stocked), 800 mA (≥5× our 3.3V load), 1.1V dropout fits 5→3.3 with margin, SOT-223 is a good heat spreader and trivially hand-solderable. A "better" LDO buys nothing here: the 3.3V load is ~100 mA and the input is regulated 5V already. |
| Analog rail | Ferrite bead + 1µF/100nF into VDDA | Keeps LED switching noise out of the ADC reference. |

**LEDs run directly on the (protected) 5V input rail**, not through the LDO — that's the whole reason the wheel is fed 5V.

### 3.2 Dash (12V vehicle in → 5V → 3.3V)

| Function | Part | Why |
|---|---|---|
| Input protection | **SMBJ33A** TVS, series **2A polyfuse**, reverse-polarity **P-MOSFET** (DMP3056L or similar, drain-to-battery orientation) | Automotive 12V sees load dump/inductive transients; 33V standoff TVS clamps them below the buck's 36V rating. P-FET reverse protection costs ~20 mV (vs 500 mV for a diode) — on 12V we can afford a FET and it protects the whole board. |
| 5V rail (3A) | **TI LMR33630ADDAR** sync buck, 400 kHz | 36V max input survives clamped transients with margin (first-principles: TVS clamping voltage ≈ 45V pk for µs — LMR33630 abs max 42V is tight, so the TVS is SMBJ26A **if** measured clamp exceeds this; default SMBJ33A + verify with scope, see rigor file). 3A covers: wheel aux feed (≤1.5A) + 5V sensors + margin. Fixed-frequency sync buck = predictable EMI, no diode drop. Alternative if JLC stock fails: TPS54331 (hand-solder SO-8). |
| 3.3V rail (2A) | **AP63203WU-7** fixed 3.3V sync buck (TSOT-26) | The dash 3.3V load is dominated by the display backlight driver (~1.2A at 100% on the 1000-nit panel) — an LDO from 5V would burn ~2W; a buck keeps dissipation <0.2W. 3.8–32V input; fed from the 5V rail. JLC-stocked. |
| 5V AUX out (wheel feed) | 1.5A polyfuse + SMBJ5.0A at the connector | The dash is the wheel's power source; the aux output must not let a wheel/harness fault kill the dash. |

## 4. Human interface

### 4.1 Thumb encoders (2–4) — Panasonic EVQ-WK4001

**Selected:** **Panasonic EVQ-WK4001** — edge-drive thumbwheel encoder, 15 PPR / 15 detents,
**integrated push switch**, vertical mount. Digi-Key/Mouser stocked (~$3–4). Panasonic lists the EVQ-WK
family under *automotive encoders* — this exact style is what OEM steering-wheel audio thumbwheels use.

**Justification:** a thumb encoder must be actuatable with the wheel gripped — that demands an
edge-drive (wheel sticking out of the faceplate) geometry, not a shaft knob. Detented quadrature
gives absolute click-count confidence; the push switch adds a "select/confirm" input for free.
Through-hole legs take shear loads a driver's thumb applies (SMD-only jog encoders rejected for
mechanical robustness).

### 4.2 Faceplate encoders (2) — Bourns PEC11H

**Selected:** **Bourns PEC11H-4120F-S0020** (20 detent, high-detent-force, flatted shaft, with push
switch option). Digi-Key/Mouser stocked.

**Justification:** faceplate rotaries are turned with gloves in a vibrating car — the *high detent
force* variant (PEC11**H**) exists precisely so vibration and glove brush-contact can't advance the
switch. 100k-cycle life, -20…+70°C (in-cockpit OK), threaded bushing panel-mounts through the
faceplate so torque loads go into the faceplate, not solder joints. 20 detents/rev maps cleanly to
map-selection UIs.

### 4.3 Buttons (6) — sealed tactile + remote option

**Selected:** 6 × **C&K KSC4** series sealed (IP67) tactile switches, PCB-mount, actuated through the
faceplate with printed plungers; plus 2 × JST-GH aux connectors so any button position can instead be
a panel-mount switch wired to the PCB.

**Justification:** IP67 sealing because a wheel gets sweat, rain, and cleaning spray. Silicone-dome
sealed tactiles have defined actuation force (choose 3.5–5N so bumps can't false-trigger). The JST-GH
alternates cost nothing and de-risk the mechanical design (button placement can change without a
board respin). Debounce is firmware, conditioning is hardware (see §6).

### 4.4 Paddles

Paddle switches are **not inputs to the MCU's function** — they pass through the wheel connector
directly to Nexus R5 digital inputs (requirement). The PCB provides: pass-through routing,
**SMAJ24CA bidirectional TVS** per line at the connector, and a **100kΩ series high-impedance sense
tap** to an MCU pin (so the display can show shift events, and so the *sim variant* can read the
paddles with internal pull-ups — same copper, two uses). The sense tap cannot load or damage the
ECU circuit by design: 100k in series means <50µA of influence at any rail.
Recommended paddle switch (off-board, mechanical): Honeywell/ZF snap-action microswitch (V15/D4 class), normally-open to GND.

## 5. Lighting

**Selected:** **WS2812B-2020** addressable RGB (LCSC C965555, ~$0.05/pc, 35k+ in stock, JLC-assemblable).
- Shift bar: **16 LEDs** in an arc/row at the wheel top edge.
- TC bar: **4 LEDs**; Lockup (diff/clutch) bar: **4 LEDs**. Total 24.
- Data driven from a G474 timer+DMA pin through a **74AHCT1G125** (5V-supplied buffer whose TTL input
  thresholds, VIH = 2.0V, legally accept 3.3V logic — this is the *correct* 3.3→5V shift, not a hack;
  WS2812 VIH is 0.7·VDD = 3.5V which 3.3V logic cannot guarantee directly).
- One 100nF per LED + 2×100µF bulk; data series resistor 300Ω at the driver.

**First-principles justification:**
- One MCU pin controls all 24 LEDs including per-LED color → shift strategy (progressive bar → flash),
  TC intervention intensity, and brightness are pure firmware. Any discrete-LED design needs
  constant-current drivers (TLC59xx) + more routing for less flexibility.
- 2020 package gives a dense 16-LED bar in <35 mm — reads as a continuous bar, like commercial wheels.
- **Sunlight math (be honest):** a bare LED die of any brand is visible in sun only if it subtends
  enough contrast against ambient. Mitigations that are part of this design, not optional: recess the
  bars ~3 mm behind a matte-black shroud (kills specular washout), run 100% duty full-saturation
  colors (pure red/green/blue, never pastel), and firmware-flash at threshold (motion beats
  luminance for eye-catch). If track testing still finds it weak, the fallback is PLCC-3535 discrete
  LEDs + CC drivers at 60 mA/die — footprint provision noted in the Altium doc as a do-not-implement-yet option.
- **Current budget (worst case, all 24 full white):** 24 × ~36 mA ≈ 0.86A. Firmware enforces a global
  cap (≤50% aggregate) making the realistic max ≈ 0.45A; the shift pattern never exceeds ~16 LEDs of
  single-color → ≈ 0.2A typical. Numbers carried into the architecture doc's power budget.
- Automotive note: WS2812B-2020 is rated -40…+85°C. It is a consumer part; the rigor file mandates a
  thermal soak + vibration shake of the assembled bar before the design is trusted.

## 6. Input conditioning (encoders, buttons — the "no damage" requirement)

Per encoder A/B/push line and per button line:
- **Series 1kΩ** at the switch → limits any injected current into the MCU pin to <5 mA even with a
  direct short to 5V (the MCU's injection spec limit).
- **100nF to GND** at the MCU side → RC with the internal/external pull-up gives ~100µs of hardware
  debounce/ESD energy absorption and RF filtering.
- **10kΩ pull-up to 3.3V** (external, not internal — defined state even in MCU reset).
- Lines that leave the PCB (JST aux buttons) additionally get a **BAV99 clamp pair to 3.3V/GND** at
  the connector.
This is the textbook R-C-clamp ladder: impedance first (limits energy), storage second (absorbs
the fast edge), clamps third (bound the voltage). It appears identically on the dash's local inputs.

## 7. Displays

### 7.1 Wheel — Sharp LS013B7DH05 memory-in-pixel LCD (1.26", 144×168)

Digi-Key stocked (~$15). Connects via 10-pin 0.5 mm FPC.
- **Reflective = sunlight-proof by physics**: it modulates *reflected* ambient light, so direct sun
  *increases* contrast. Every emissive option (OLED/TFT) at this size is invisible at noon or needs
  a >600 cd/m² backlight the 5V budget can't spare.
- **~50 µW** typical power — rounding error in the wheel budget.
- 3-wire SPI + EXTCOMIN toggle; trivial firmware; updates at any rate we like (encoder values don't
  need video rates).
- 1.26" matches the requested Moza-KS-class size.
- Trade-off accepted: monochrome, no night backlight. Night running (FSAE runs in daylight) would
  need the frontlight variant (Azumo laminated versions exist) — noted, not selected.

### 7.2 Dash — Riverdi RVT50HQBNWN00 (5.0", 800×480, **1000 cd/m² IPS**, EVE4 BT817)

Riverdi-direct / Mouser (~$150–190 class).
- **1000 cd/m²** is the entry point for direct-sunlight color TFT legibility (typical hobby TFTs:
  250–350 cd/m² — unreadable on a grid walk).
- **BT817 EVE coprocessor**: the G474 sends drawing *commands* over SPI/QSPI instead of pixels —
  no frame buffer, no RGB bus, no STM32H7, no LVGL port. An 800×480 dash UI runs from a 170 MHz M4
  with single-digit-% CPU. This is the whole reason the dash can share the wheel's MCU platform
  (one platform = one codebase, per principle #6).
- 3.3V supply, integrated backlight driver (≈1.2A at full brightness — drives the 3.3V buck sizing).
- Non-touch selected deliberately: gloved drivers don't use capacitive touch; buttons/encoders do it better.

## 8. Connectors

| Location | Part | Why |
|---|---|---|
| Wheel ↔ car | **Deutsch DTM 6-way** (DTM04-6P panel side / DTM06-6S harness) | Exactly 6 circuits needed (5V, GND, CANH, CANL, PADDLE_UP, PADDLE_DOWN) — the connector *is* the requirement stated in copper. Keyed (reverse-plug impossible → enables the no-series-diode 5V decision), gold sockets, sealed, crimped, motorsport-standard, ~$8/pair. Autosport/ASL is the pro upgrade; 10× the cost, zero functional gain at FSAE loads. Wheel quick-release passthrough: use a 6-way coiled cord or through-hub contacts with the same 6 circuits. |
| Dash ↔ car | **Deutsch DTM 12-way** ×2 | One power/bus connector (12V, GND, CANH, CANL, 5V_AUX_OUT + return for the wheel feed), one DAQ connector (8 × AIN + 5V sensor supply + GNDs). Same crimp tooling as the wheel — one tool, one spares kit. |
| PCB-internal (display FPC) | 10-pin (wheel) / 20-pin (dash) 0.5 mm FPC ZIF | Dictated by the display modules. |
| Aux buttons / paddles on wheel PCB | **JST-GH** (1.25 mm, positive lock) | Locking (vibration), tiny, cheap, JLC-stocked. Never use unlocked 2.54 mm headers in a vehicle. |
| Debug | **Tag-Connect TC2030-CTX** footprint (no connector cost) + USB-C | SWD access with zero BOM cost and no connector to vibrate loose. |
| Sim/DFU | **USB-C 16-pin** (e.g. HRO TYPE-C-31-M-12, LCSC C165948) + USBLC6-2SC6 ESD + 5.1k CC pull-downs | USB 2.0 FS only; C because nobody should buy a micro-B cable in 2026. |

## 9. Availability summary (checked July 2026)

| Part | Source | Status |
|---|---|---|
| STM32G474RET6 | LCSC C521608 / JLC | In stock, ~$3.93 |
| TJA1051T/3 | LCSC / Digi-Key | In stock (SIT1051T/3 fallback) |
| WS2812B-2020 | LCSC C965555 / JLC | 35k+ stock, ~$0.05 |
| AMS1117-3.3 | JLC **basic** | Always stocked |
| LMR33630ADDAR / AP63203WU-7 | LCSC / JLC | In stock |
| LS013B7DH05 | Digi-Key | In stock, ships same day |
| RVT50HQBNWN00 | Riverdi / Mouser | In stock (lead-time risk: single-source — order early, see rigor file) |
| EVQ-WK4001 | Digi-Key / Mouser / Newark | In stock |
| PEC11H series | Digi-Key / Mouser | In stock |
| DTM connectors | Digi-Key / motorsport suppliers | Commodity |

Hand-solder fallback exists for every non-JLC part (all are leaded packages or module-level).

## Sources

- [Haltech CAN Broadcast Protocol spec (V2)](https://cdck-file-uploads-europe1.s3.dualstack.eu-west-1.amazonaws.com/arduino/original/3X/2/5/257e09c05863159d7eee557d14ac71bb658bc5ba.pdf) — bus rate/format
- [STM32G474RET6 — LCSC C521608](https://www.lcsc.com/product-detail/C521608.html) · [JLCPCB listing](https://jlcpcb.com/partdetail/STMicroelectronics-STM32G474RET6/C521608)
- [WS2812B-2020 — LCSC C965555](https://www.lcsc.com/product-detail/Light-Emitting-Diodes-LED_Worldsemi-WS2812B-2020_C965555.html) · [datasheet](https://www.mouser.com/pdfDocs/WS2812B-2020_V10_EN_181106150240761.pdf)
- [Sharp LS013B7DH05 — Digi-Key](https://www.digikey.com/en/products/detail/sharp-microelectronics/LS013B7DH05/5799456) · [spec PDF](https://pages.azumotech.com/hubfs/Sharp%20Spec%20Sheets/1.26_Sharp-LCD-Specification-LS013B7DH05.pdf)
- [Riverdi RVT50HQBNWN00 (EVE4 BT817, 1000 cd/m²)](https://riverdi.com/product/eve4-intelligent-display-rvt50hqbnwn00-5-inch)
- [Panasonic EVQ-WK4001 — Digi-Key](https://www.digikey.com/en/products/detail/panasonic-electronic-components/EVQ-WK4001/412477) · [EVQWK datasheet](https://media.digikey.com/pdf/Data%20Sheets/Panasonic%20Electric%20Works%20PDFs/EVQWK.pdf) · [Panasonic automotive encoder lineup](https://na.industrial.panasonic.com/products/switches-encoders-interface-devices/encoder-potentiometers/lineup/automotive-encoders/series/81850/model/81834)
- [Bourns PEC11H datasheet](https://www.bourns.com/docs/Product-Datasheets/PEC11H.pdf)
