# Hardware Selections — FSAE CAN Steering Wheel, Dash, and Sim Variant

> **Permanent memory file.** Every part here was chosen from first principles. If a part is ever
> substituted, the substitution must satisfy the same *reasoning*, not just the same footprint.
> Availability was checked July 2026 (LCSC/JLCPCB for PCBA-priority parts, Digi-Key/Mouser for the rest).

## 0. Selection principles (read first)

1. **The wheel sees exactly 5 electrical functions at its connector: +12V, GND, PADDLE_UP, PADDLE_DOWN, CAN (H+L).**
   Every wheel part must live within that constraint — no extra signal wires, no dependency on any
   other board. (Rev A used a 5V feed from the dash; superseded — see §0.2 and lesson L9.)
2. **First-principles power reasoning:** the wheel runs from **vehicle 12V** and regulates locally.
   Two reasons it is *not* fed a pre-regulated 5V rail from the dash:
   (a) **Independence** — a 5V feed made the wheel unable to function without the dash board present
   and healthy, coupling two products that have no functional reason to be coupled.
   (b) **Contact-resistance tolerance** — the wheel's power crosses slip-ring / quick-release
   contacts that oxidize and wear. That is the real-world failure mode, not steady-state drop. At
   5V/0.63A, a set of contacts degrading to 500 mΩ costs 315 mV out of ~600 mV of LDO headroom
   (half the margin, gone). At 12V the same degradation costs 145 mV out of ~7V — unmeasurable. 12V
   also halves the current through those wearing contacts (≈0.29A vs 0.63A).
   **Never wire either board's power to an ECU sensor-supply pin** — those are sized for sensors and
   shared with them; the wheel's LED load would brown out the sensors along with itself.
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
- Both boards have a local 5V rail, so the 5V supply costs nothing.
- 1 Mbit/s (Haltech bus rate) is well within its 5 Mbit/s rating; ±58V bus fault tolerance survives
  harness mis-wiring to battery.
- `/3` silent-mode pin tied off; no standby logic to get wrong.

**Bus conditioning (both boards):** PESD2CAN (or NUP2105L) dual-line TVS on CANH/CANL, optional
split termination footprint (2×60.4Ω + 4.7nF to GND, **DNP by default** — see architecture doc for
termination policy), common-mode choke footprint (DNP unless emissions testing demands it).

## 3. Power

### 3.1 Wheel (12V in → 5V → 3.3V)

| Function | Part | Why |
|---|---|---|
| Input protection | **SMBJ33A** TVS + **1A polyfuse** + **P-MOSFET** reverse protection (DMP3056L class) | The wheel is now fed raw vehicle 12V, so it sees the full automotive transient environment (load dump, inductive kicks) that the dash's regulation used to absorb for it. This is the **same proven stack as the dash §3.2** — copy the circuit, don't reinvent it. **The move to 12V pays for reverse protection**: at 5V a series diode's 0.3–0.5V was 6–10% of the rail and was rejected; at 12V a P-FET costs ~20 mV, so the wheel gains protection Rev A did not have. |
| 5V rail | ⚠ **AP63205WU-7 REJECTED — see below.** Replacement: a **60 V-class buck** (candidate: TI **LMR36015**, 4.2–60 V, tolerant to 66 V, 1.5 A, LCSC `C2863325`) | An LDO is impossible here: 12→5V at 0.63A would dissipate 0.63 × 7 ≈ **4.4 W** in a handheld part; a sync buck dissipates ~0.35 W. **But the AP63205's absolute maximum VIN is 35 V DC (40 V for 400 ms), and the SMBJ33A in the row above clamps at 53.3 V — so the TVS would let the buck die before it did its job.** A 35 V converter cannot sit directly across a vehicle battery. With a 60 V part the SMBJ33A's clamp has ~7 V of margin and its 33 V standoff still clears a jump start. Full arithmetic: `datasheet-verification.md` §5. **The LMR36015 is a candidate, not yet a decision — its datasheet must be verified before adoption (rule 1a).** |
| 3.3V rail | **AMS1117-3.3** (SOT-223) from the 5V rail | Unchanged and still correct: the 3.3V load is ~100 mA off an already-regulated 5V → 0.17 W dissipation. JLC *basic* part, hand-solderable. Adding a second buck here would buy nothing. |
| Analog rail | Ferrite bead + 1µF/100nF into VDDA | Keeps buck + LED switching noise out of the ADC reference. |
| USB (sim) | BAT60A Schottky OR-ing VBUS into the **5V rail, downstream of the buck** | Lets the sim variant run from USB with the buck unpopulated — see `sim-variant-instructions.md`. |

**LEDs run on the local 5V rail**, not through the LDO.

**Layout consequence (do not ignore):** the buck adds a switching node ~15×15 mm inside a small board
that also carries a memory LCD SPI, encoder lines, and an 800 kHz WS2812 data line. Place it in the
connector corner, keep the hot loop <20 mm², and keep it clear of the quick-release hub keep-out. The
LED data line remains the board's worst aggressor, so this is a manageable addition, not a new class of problem.

### 3.2 Dash (12V vehicle in → 5V → 3.3V)

| Function | Part | Why |
|---|---|---|
| Input protection | **SMBJ33A** TVS, series **2A polyfuse**, reverse-polarity **P-MOSFET** (DMP3056L or similar, drain-to-battery orientation) | Automotive 12V sees load dump/inductive transients; 33V standoff TVS clamps them below the buck's 36V rating. P-FET reverse protection costs ~20 mV (vs 500 mV for a diode) — on 12V we can afford a FET and it protects the whole board. |
| 5V rail (~1.2A) | ⚠ **LMR33630 REJECTED — 42 V abs max vs a 53.3 V TVS clamp.** Replacement: the **same 60 V candidate as the wheel** (TI **LMR36015**, 4.2–60 V, 1.5 A, LCSC `C2863325`) | This is the board's only high-voltage-exposed converter. Load re-derived after reading the Riverdi datasheet: the backlight is on its own **BLVDD/5 V** rail (353 mA), not 3.3 V, so the 5 V rail carries backlight 353 mA + sensor excitation 200 mA + servo buffers + CAN ≈ 0.6 A, plus the 3.3 V rail reflected (≈0.37 A) ≈ **1.0 A**. A 1.5 A part fits comfortably — so **one converter part covers both boards**, which the earlier (wrong) 1.6 A figure had ruled out. Candidate pending its own datasheet verification (rule 1a). |
| 3.3V rail (2A) | **AP63203WU-7** fixed 3.3V sync buck (TSOT-26) — **now fed from `+5V`, NOT `+12V_P`** | The dash 3.3V load is dominated by the display backlight driver (~1.2A at 100% on the 1000-nit panel — figure still unverified) — an LDO from 5V would burn ~2W; a buck keeps dissipation <0.2W. Rated 3.8–32 V operating, **35 V absolute max** — which is why it must *not* face the battery. Re-sourced from the 5 V rail, where 35 V is irrelevant, it is a perfectly sound part. This keeps the 60 V front-end converter as the board's **single point of high-voltage exposure**. |
| ADC clamps (DAQ front end) | **BAV199**, *not* BAT54S | A Schottky leaks 2 µA at 25 °C and ~100 µA at 100 °C; across the AFE's 5 kΩ Thévenin source that is 10 mV of offset cold and **500 mV hot** — a 20 % error on a 0–2.5 V channel, in a dash specified for direct sun. BAV199 leaks ~3 pA. See `datasheet-verification.md` §5A. |
| ~~5V AUX out~~ | **Deleted in Rev B** | The wheel is now 12V-fed and standalone. Removing the aux output deletes its polyfuse, TVS, and two connector positions, and lets the 5V buck shrink. Recorded as lesson L9. |

## 4. Human interface

### 4.1 Thumb encoders (2–4) — Bourns PEC09 (right-angle)

> **Rev B correction.** The original selection (Panasonic EVQ-WK4001 edge-drive thumbwheel) is
> **obsolete** — Mouser lists it End-of-Life/NCNR. This is not a one-part problem: **the entire
> edge-drive thumbwheel category has been discontinued.** EVQ-WK is dead, Panasonic's EVQWGD001
> "roller encoder" is dead (remaining supply is old stock), and the only functional survivor is the
> Grayhill 62T at >$100 and far too large. **Do not design around an edge-drive thumbwheel.**
> Recorded as lesson L14.

**Selected:** **Bourns PEC09-2120F-S0012** — 9 mm **right-angle** incremental encoder, 12 PPR /
12 detents, integrated push switch, 6 mm flatted shaft, through-hole PC pins.
Digi-Key: **Active, 871 in stock**, $3.96 @1 / $2.56 @100 (checked July 2026). Shaft-length variants
`-2115F-` (15 mm) and `-2125F-` (25 mm) exist; 24-detent option is the `-22xxF-` prefix.
*(Note: the 25-week figure on DigiKey is Bourns' factory lead time for restock, not the shipping
time — stocked variants ship immediately. Order a stocked variant, not a tray-pack `T00xx` part.)*

**Justification:**
- **Right-angle pins are the geometric requirement.** With the main PCB parallel to the faceplate, a
  right-angle encoder's shaft exits *parallel* to the board — i.e. out of the wheel's top/side edge,
  where a thumb reaches while gripping. A vertical encoder points at the driver, which is correct for
  faceplate knobs (§4.2) and wrong for thumb controls.
- **Rotational life is a non-issue here despite the modest number.** 30,000 cycles is *revolutions*,
  and at 12 detents that is ~360,000 detent clicks. An FSAE season is on the order of 10,000 detents,
  so this is roughly two orders of magnitude of headroom. Do not pay for 100k-cycle parts on this axis.
- Detented quadrature gives absolute click-count confidence; the push switch adds a select/confirm
  input for free; through-hole pins take the shear load a thumb applies.

**Mechanical note:** the 6 mm shaft needs a small knurled thumbwheel or knob. Specify it with the
faceplate design; the shaft-length variant follows from how deep the board sits behind the panel.

#### 4.1b Escape hatch: satellite encoder boards (use if geometry fights you)

If right-angle geometry does not place the thumbs where ergonomics wants them, mount the thumb
encoders on **small satellite PCBs** connected to the main board by JST-GH cable (the same
conditioning cell as §6, run at the satellite end). This is the more robust architecture and is worth
choosing deliberately, not just as a fallback:

- **Thumb position becomes an ergonomics decision, not a board-outline decision** — the real
  constraint on a steering wheel is grip geometry, which no main-board shape will naturally satisfy.
- **Obsolescence becomes cheap.** When the next encoder goes EOL — and §4.1's history says it will —
  a satellite respin is a $5 board, not a main-board spin. Given that this exact part category just
  died under us, that insulation has demonstrated value.
- **One part number everywhere:** a satellite lets a *vertical* encoder point any direction, so all
  six encoders can be the PEC11H of §4.2 — one footprint, one spares kit, better volume pricing.

Cost: two extra connectors and two cable assemblies per wheel, plus lines that leave the board
(so the BAV99 clamp of §6 applies at each satellite connector).

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

### 7.1 Wheel — Sharp LS027B7DH01 memory-in-pixel LCD (2.7", 400 × 240, mono)

**Selected (Rev B.11)**, replacing the JDI LPM013M126A (1.28", 176 × 176, 8 colours), which had itself
replaced the Sharp LS013B7DH05. Driver: **aspect ratio and legible size.**

- **Reflective = sunlight-proof by physics**, unchanged and still the whole argument. A memory-in-pixel
  LCD modulates *reflected* ambient light, so direct sun *increases* contrast. Every emissive option at
  this size is either invisible at noon or needs a backlight that dominates the wheel's power budget.
- **Landscape 5:3, and four times the picture.** Active area **58.8 × 35.28 mm** against roughly
  28 × 28 mm — the thing a driver reads at a glance mid-corner got substantially bigger, and it is now
  the shape a gear number plus a couple of values actually wants.
- **400 × 240 mono beats 176 × 176 in 8 colours for this job.** Colour on the wheel was never doing
  much work: the shift lights are the WS2812 bar, not the panel. Resolution and size are what
  legibility is made of.
- **350 µW** maximum — still a rounding error in the budget.
- **Sourcing improves, which was the JDI part's one real weakness.** LCSC stocks it (`C17492463`), so
  it is JLC-assemblable instead of specialty-distributor-only.
- **Pinout is identical** to both parts it replaces (10-pin FPC: SCLK, SI, SCS, EXTCOMIN, DISP, VDDA,
  VDD, EXTMODE, VSS, VSSA), so the MCU nets and pin assignments do not move at all.

**Constraints the datasheet imposes** (spec LCP-2110015A, in `hardware/lib/`):
**VDD/VDDA = +4.8 / 5.0 / 5.5 V** — a **5 V** panel, so the display rail moves off `+3V3`. But
**V_IH min = 2.70 V**, so **3.3 V logic drives it directly with no level shifting** — the exact
inverse of the JDI part's `V_IH = VDD − 0.1 V` rule, which had forced the panel onto the MCU's rail.
⚠ **`EXTMODE` is the exception and must go to `+5V`**, not 3.3 V. **SCLK ≤ 2 MHz.** Gate address is
**8-bit LSB-first**, not the JDI's 10-bit MSB-first.

**Three honest downsides.**
1. **Mono.** That is the trade, made deliberately.
2. **Operating range is still −20 … +70 °C** — no better than before, so **assumption A9 stands
   unchanged**, and it now also covers the encoders (A11).
3. **The zero-change fallback is gone.** The LS013B7DH05 is a **3 V** part (3.6 V absolute max) and
   would be destroyed on this board's 5 V display rail — identical pinout notwithstanding (defect
   8.24). Mitigation is now a **spare unit** plus LCSC stock, rather than a fallback footprint.

**Mechanical cost, which is the real price of this change:** 62.8 × 42.82 mm module versus roughly
28 mm square, a much larger faceplate cutout, and the bezel must **shade the non-active border** —
the datasheet requires the gate driver be kept out of the light.

### 7.2 Dash — Riverdi RVT50HQBNWN00 (5.0", 800×480, **1000 cd/m² IPS**, EVE4 BT817)

Riverdi-direct / Mouser (~$150–190 class).
- **1000 cd/m²** is the entry point for direct-sunlight color TFT legibility (typical hobby TFTs:
  250–350 cd/m² — unreadable on a grid walk).
- **BT817 EVE coprocessor**: the G474 sends drawing *commands* over SPI/QSPI instead of pixels —
  no frame buffer, no RGB bus, no STM32H7, no LVGL port. An 800×480 dash UI runs from a 170 MHz M4
  with single-digit-% CPU. This is the whole reason the dash can share the wheel's MCU platform
  (one platform = one codebase, per principle #6).
- **Two supplies, verified from DS Rev 1.7:** module logic `VDD` = 3.3 V at **98 mA typ / 384 mA max**, and a *separate* backlight rail `BLVDD` (3.1 / **5.0 typ** / 5.5 V) drawing **353 mA at 5 V** at full brightness. The earlier "1.2 A on the 3.3 V rail" was wrong on all three counts — see `datasheet-verification.md` §5B. Feed BLVDD from 5 V: at 3.3 V the constant-current driver would pull 657 mA for the same light.
- 20-pin 0.5 mm FFC ("RiBUS"), matched cable `FFC0520150`; VIH 2.0 V so 3.3 V logic drives it directly; INT and RST/PD are internally pulled up 47 kΩ.
- Non-touch selected deliberately: gloved drivers don't use capacitive touch; buttons/encoders do it better.

## 8. Connectors

| Location | Part | Why |
|---|---|---|
| Wheel ↔ car | **Deutsch DTM 6-way** (DTM04-6P panel side / DTM06-6S harness) | Exactly 6 circuits needed (**12V**, GND, CANH, CANL, PADDLE_UP, PADDLE_DOWN) — the connector *is* the requirement stated in copper. Keyed and sealed, gold sockets, crimped, motorsport-standard, ~$8/pair. Autosport/ASL is the pro upgrade; 10× the cost, zero functional gain at FSAE loads. Wheel quick-release passthrough: a 6-way coiled cord or through-hub contacts carrying the same 6 circuits. **Rev B:** with 12V on pin 1, reverse protection is a P-FET on the board (§3.1) rather than relying on connector keying alone — belt and braces, since a quick-release is mated by a driver in a hurry. |
| Dash ↔ car | **Deutsch DTM 12-way** ×2 | J1 power/bus/servo-signal (12V, GND, CANH, CANL, SERVO1_PWM, SERVO2_PWM — the last two reuse the positions freed by deleting the Rev A `5V_AUX_OUT`); J2 DAQ (6 × AIN, 2 × ARB position feedback, 5V sensor supply, 3 × sensor GND). Same crimp tooling as the wheel — one tool, one spares kit. **Servo power never crosses either connector** (§9.1). |
| PCB-internal (display FPC) | 10-pin (wheel) / 20-pin (dash) 0.5 mm FPC ZIF | Dictated by the display modules. |
| Aux buttons / paddles on wheel PCB | **JST-GH** (1.25 mm, positive lock) | Locking (vibration), tiny, cheap, JLC-stocked. Never use unlocked 2.54 mm headers in a vehicle. |
| Debug | **Tag-Connect TC2030-CTX** footprint (no connector cost) + USB-C | SWD access with zero BOM cost and no connector to vibrate loose. |
| Sim/DFU | **Wheel `J2`: Molex `204711-0001`, VERTICAL mount.** Dash `J3`: HRO TYPE-C-31-M-12, LCSC C165948. Both + USBLC6-2SC6 ESD + 5.1 k CC pull-downs | USB 2.0 FS only; C because nobody should buy a micro-B cable in 2026. **The wheel changed to a vertical part (user, 2026-07) for packaging** — it exits perpendicular to the board instead of needing a clear run to a board edge, which is easier to fit behind the faceplate. ⚠ The two boards therefore use **different USB-C connectors and different footprints**; the shared library carries both. ⚠ Molex is a Digi-Key/Mouser line and JLC assembles from LCSC — check availability before the BOM is frozen. |

## 9. Dash servo outputs — adjustable anti-roll bar (2 channels)

**Requirement:** two outputs on the dash to drive servo-actuated ARBs (front and rear).

### 9.1 Architecture decision: signal-only, servo power external

**Selected:** the dash sources **2 × buffered 5V PWM signal lines + ground reference**. Servo power
comes from a **separate 12V→7.4V BEC/regulator in the harness**, independently fused.

**First-principles justification:** high-torque digital servos of the class used for ARB actuation
(Savox SB-2290SG and equivalents) draw ~5A stall at 7.4V. Two channels is ~10A / **~74W**. Putting
that through a dash PCB means a large buck, a heatsink, and 10A of copper running beside a display
and an 8-channel analog front end — all to save one harness branch. Signal-only keeps the dash small,
cool, and analog-quiet, and lets the servo supply be fused and killed independently of the dash.
**A DTM pin is rated 7.5A — 10A of servo current must not cross this connector either.**

> **Ground rule (classic failure):** the servo supply's ground **must** star back to dash ground.
> PWM is referenced to the dash's ground; if the servo BEC grounds elsewhere, the reference floats
> and pulse widths are interpreted wrong or jitter. Signal + GND travel together to each servo.

### 9.2 Output stage (per channel)

`MCU TIM4_CHx (3.3V PWM) → 74AHCT2G125 (dual buffer, 5V) → 100 Ω series → SMAJ5.0A clamp → connector`

| Part | Why |
|---|---|
| **74AHCT2G125** dual buffer, 5V | Servos expect a ~5V logic pulse; **AHCT** TTL input thresholds (VIH 2.0V) legally accept 3.3V logic — the identical, correct level-shift used for the WS2812 chain (§5). Buffering means a harness fault damages a $0.15 buffer, not the MCU. |
| **100 Ω series** | Fault-current limiter and transmission-line damper. A 12V short onto a servo line pushes only (12−5)/100 ≈ **70 mA** into the clamp — survivable — instead of destroying the buffer. Also damps ringing on a multi-metre run to the suspension. |
| **SMAJ5.0A** at the connector | These lines run the length of the car near suspension and wiring looms; clamp transients at the connector before they travel. |

- **MCU pins:** `PB6`/`PB7` = TIM4_CH1/CH2 — two channels off one timer, so both servos share a
  timebase. Standard 50 Hz frame (1.0–2.0 ms pulse); firmware may raise to 333 Hz for digital servos.
- **Position feedback:** reassign **AIN7/AIN8** of the existing DAQ front end (§ dash instructions) to
  ARB position sensors — servo pot tap or an external rotary sensor on the bar. Costs **zero new parts**
  (0–5V front end already fits) and enables the divergence alarm in §9.3.
- **Connector:** the two signals take **J1 pins 5–6** — exactly the positions freed by deleting the
  Rev A `+5V_AUX` wheel feed, referenced to J1's power ground. They are deliberately **not** on the DAQ
  connector: eight analog returns share J2's grounds, and any current in a shared return becomes offset
  on every DAQ channel. A PWM signal must also reference the ground its receiver uses, and the servo
  BEC stars to power ground. No new connector either way.

### 9.3 Safety requirements (firmware — these are requirements, not preferences)

1. **Hold last commanded position on CAN loss.** Never spring to a default or centre. An ARB step
   change mid-corner is a handling event; a frozen bar is merely a car with a fixed setup.
2. **Firmware endstops** on commanded pulse width (configurable min/max per channel). A servo driven
   against a mechanical hard stop draws stall current indefinitely and burns out — this is the single
   most likely way to destroy a servo.
3. **Slew-rate limit** (full travel no faster than ~2 s) so a knob spin can't slam the linkage.
4. **Divergence alarm:** if commanded vs. measured position differ beyond threshold for >500 ms,
   flag on the dash and log — this is how a seized linkage, stripped spline, or dead servo announces itself.
5. **Servo supply independently fused and killable** without taking the dash down.

**Open mechanical question (tracked as A7):** whether the ARB mechanism back-drives when servo power
is lost. A self-locking worm drive holds its setting; a direct lever does not. This determines
whether losing servo power is a non-event or a mid-session setup change, and it is a mechanical-team
answer, not an electronics one.

## 10. Availability & lifecycle validation (re-checked July 2026)

Lifecycle status is checked here, not just stock — **a part can be in stock and still be obsolete**,
which is exactly how the EVQ-WK4001 got into Rev A (lesson L14).

| Part | Source | Lifecycle | Stock / notes |
|---|---|---|---|
| STM32G474RET6 | LCSC C521608 / JLC | Active | In stock, ~$3.93 |
| TJA1051T/3 | LCSC / Digi-Key | Active | In stock (SIT1051T/3 second source) |
| WS2812B-2020 | LCSC C965555 / JLC | Active | 35k+ stock, ~$0.05 |
| AMS1117-3.3 | JLC **basic** | Active | Always stocked |
| LMR33630ADDAR / AP63203WU-7 / AP63205WU-7 | LCSC / JLC | Active | In stock |
| 74AHCT1G125 / 74AHCT2G125 | LCSC / JLC | Active | In stock (commodity logic, many second sources) |
| DMP3056L (reverse-polarity P-FET) | LCSC / Digi-Key | Active | In stock; any logic-level P-FET ≥30 V works |
| **PEC09-2120F-S0012** (thumb, right-angle) | Digi-Key | **Active** | **871 in stock**, $3.96/1, $2.56/100. 30k cycles = ~360k detents, ample (§4.1) |
| PEC11H-4120F-S0020 (faceplate) | Digi-Key / Mouser / TME | **Active** | In stock, ~$2.50–3.10, 100k cycles, −20…+70 °C |
| ~~EVQ-WK4001~~ | — | **OBSOLETE (EOL/NCNR)** | **Removed in Rev B.** Whole edge-drive thumbwheel category is discontinued — see §4.1 |
| C&K KSC4 sealed tactile | Digi-Key / C&K | Active | Current C&K catalogue part, IP67, 5.2 mm |
| **LS013B7DH05** (wheel display) | Digi-Key / **LCSC C17500193 / JLC** | Active **with a materials PCN** | In stock (~$6.93 LCSC, ~$15 DK). Sharp issued a change notice on polarizer surface treatment / adhesive — **not** a discontinuation, but re-verify the optical spec of the revision actually delivered (ties to lesson L6) |
| **RVT50HQBNWN00** (dash display) | Riverdi direct / RS / Mouser | Active | ⚠ **Thinnest supply line in the BOM** — distributor stock seen in low single digits. Single-source, long-lead. **Buy the dash displays first, before any PCB is ordered**, and buy a spare |
| DTM connectors | Digi-Key / motorsport suppliers | Active | Commodity |

Hand-solder fallback exists for every non-JLC part (all are leaded packages or module-level).

**Two supply actions that should happen before board money is spent:** order the Riverdi display (and
a spare) because it is single-source with thin stock, and buy encoders early enough to confirm the
mechanical fit of the shaft length you chose.

## Sources

- [Haltech CAN Broadcast Protocol spec (V2)](https://cdck-file-uploads-europe1.s3.dualstack.eu-west-1.amazonaws.com/arduino/original/3X/2/5/257e09c05863159d7eee557d14ac71bb658bc5ba.pdf) — bus rate/format
- [STM32G474RET6 — LCSC C521608](https://www.lcsc.com/product-detail/C521608.html) · [JLCPCB listing](https://jlcpcb.com/partdetail/STMicroelectronics-STM32G474RET6/C521608)
- [WS2812B-2020 — LCSC C965555](https://www.lcsc.com/product-detail/Light-Emitting-Diodes-LED_Worldsemi-WS2812B-2020_C965555.html) · [datasheet](https://www.mouser.com/pdfDocs/WS2812B-2020_V10_EN_181106150240761.pdf)
- [Sharp LS013B7DH05 — Digi-Key](https://www.digikey.com/en/products/detail/sharp-microelectronics/LS013B7DH05/5799456) · [spec PDF](https://pages.azumotech.com/hubfs/Sharp%20Spec%20Sheets/1.26_Sharp-LCD-Specification-LS013B7DH05.pdf)
- [Riverdi RVT50HQBNWN00 (EVE4 BT817, 1000 cd/m²)](https://riverdi.com/product/eve4-intelligent-display-rvt50hqbnwn00-5-inch)
- [Bourns PEC09-2120F-S0012 — Digi-Key (Active, in stock, right angle)](https://www.digikey.com/en/products/detail/bourns-inc/PEC09-2120F-S0012/2440258) · [PEC09 datasheet](https://www.bourns.com/docs/product-datasheets/pec09.pdf) · [Bourns PEC09 series page](https://bourns.com/products/encoders/details/contacting-encoders/pec09)
- [Bourns PEC11H datasheet](https://www.bourns.com/docs/Product-Datasheets/PEC11H.pdf) · [PEC11H-4120F-S0020 — Digi-Key](https://www.digikey.com/en/products/detail/bourns-inc/PEC11H-4120F-S0020/12349535)
- **Obsolete, do not use:** [Panasonic EVQ-WK4001 — Mouser (lifecycle: EOL/NCNR)](https://www.mouser.com/ProductDetail/Panasonic/EVQ-WK4001?qs=WwqriLBepZtXJ9ryWu3WQQ%3D%3D) · [Panasonic discontinued-products index](https://industry.panasonic.com/global/en/products/discontinued) — kept here as the evidence trail for lesson L14
- [Sharp LS013B7DH05 — LCSC C17500193](https://www.lcsc.com/product-detail/C17500193.html) · [JLCPCB part page](https://jlcpcb.com/partdetail/SharpMicroelectronics-LS013B7DH05/C17500193) (JLC-assemblable, ~$6.93)
- [C&K KSC4 series datasheet](https://www.ckswitches.com/media/1970/ksc4.pdf) · [C&K KSC4 product page](https://www.ckswitches.com/products/switches/product-details/Tactile/KSC4/)
- [Riverdi RVT50HQBNWN00 datasheet Rev 1.7](https://download.riverdi.com/RVT50HQBNWN00/DS_RVT50HQBNWN00_Rev.1.7.pdf)
