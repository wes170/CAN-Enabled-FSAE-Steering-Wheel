# The Wheel Board, Explained in Plain English

> This is a companion to `wheel-schematic-complete.md`, which is the file you actually draw the
> schematic from. Every part number, pin number, value and net name below is copied from that file
> — nothing here is a new decision. This file exists to explain **why** the schematic looks the way
> it does, so that when you're in Altium at 11pm and tempted to "simplify" something, you know what
> you'd be breaking.
>
> Read `wheel-schematic-complete.md` alongside this. When in doubt, the schematic file wins — this
> is the explanation, not the reference.

## 0. The big picture

The wheel board is a small computer bolted to a steering wheel. It reads six rotary knobs
("encoders"), six buttons, two paddle-shifter signals, shows information on a small screen, lights
up 24 RGB LEDs, and talks to the rest of the car over CAN bus (a two-wire differential
communication standard used throughout automotive and racing electronics — "differential" means the
signal is the *difference* between two wires, which makes it resistant to electrical noise).
Everything on the board is built around one microcontroller, `U4`, an STM32G474RET6 — a chip that
combines a fast processor core with the CAN hardware, the analog-to-digital converters (ADCs, which
turn a voltage into a number the software can read), timers, and USB, all on one 64-pin chip.

The board only has six wires reaching it from the rest of the car (see §8: 12 V power, ground, CAN
high, CAN low, and the two paddle-shift signals). Everything else — regulating that 12 V down to
voltages the chip and display can use, protecting the electronics from the car's electrically noisy
environment, and talking to all the switches and LEDs — happens locally on this board.

**The power tree**, i.e. the path voltage takes from the car's battery down to each chip, is:

```
+12V_IN → F1 (fuse) → Q1 (reverse-protection FET) → (D1 clamps spikes) → +12V_P → U1 (buck) → +5V → U2 (LDO) → +3V3 → FB1 (filter) → +3V3A
```

Keep this picture in your head through the rest of the document — every section below is one link
in that chain, or one thing hanging off a rail once it exists.

**Net names are not decoration.** A "net" is just the electrical term for "everything that's wired
together" — one continuous piece of copper (conceptually) that firmware, layout rules, and this
document all refer to by name. Type net names exactly as given (`+12V_P`, `NET_FB`, `LCD_SCLK`, and
so on) — the layout rules and the firmware pin map both depend on the names matching letter for
letter.

---

## 1. The five power rails

| Net | What it is |
|---|---|
| `+12V_IN` | Raw 12 V straight off the car's harness, before any protection — this can have spikes, dips, and reverse-polarity surprises on it |
| `+12V_P` | 12 V *after* it has passed through the fuse, the reverse-protection FET, and the transient clamp — this is the "safe" 12 V that only feeds the first regulator |
| `+5V` | The board's local 5 V, made by `U1`. Powers the LEDs, the CAN chip, the LED level-shifter, and the next regulator |
| `+3V3` | The board's local 3.3 V, made by `U2`. Powers the microcontroller, the display, and all the pull-up resistors |
| `+3V3A` | A cleaned-up, filtered copy of `+3V3` used *only* for the microcontroller's analog reference and analog supply pins, so digital switching noise doesn't corrupt the ADC readings |

There is one ground net (`GND`) in the schematic. In the physical board layout it gets split into
separate copper "pours" (poured copper regions) for analog and power sections to keep noisy digital
return currents away from sensitive analog measurements, but as far as the schematic is concerned
it's all one net.

---

## 2. Power sheet (`wheel-power.SchDoc`)

### 2.1 Input protection — the electronic gatekeeper for the raw 12 V

**What it does and why it exists.** The wheel is fed raw, unregulated 12 V straight from the car's
harness (see `hardware-selections.md` §0.2 for why the wheel gets its own regulation instead of a
pre-regulated 5 V feed from the dash — short version: independence from the dash board, and much
better tolerance of worn or oxidized connector contacts at 12 V than at 5 V). Raw automotive 12 V is
not a clean, steady 12 V — it can spike far higher during events like "load dump" (a sudden voltage
spike that occurs when the alternator is delivering current and the battery is suddenly
disconnected, e.g. a loose terminal) or get plugged in backwards by a rushed driver at a quick-release
connector. This section exists purely to survive that abuse before the raw voltage ever reaches the
regulator that makes 5 V.

**How it's built.**

| Ref | Part | Value / spec | Connections |
|---|---|---|---|
| `F1` | Resettable polyfuse (a fuse that trips open on overcurrent and resets itself once the fault clears and it cools down), 1206 package | 1.1 A hold / 2.2 A trip / 30 V | `+12V_IN` → `NET_FUSED` |
| `Q1` | **DMP3056L**, a P-channel MOSFET (a transistor used here as a one-way electronic valve for power, not a switch you toggle) in a SOT-23 package | V_DSS −30 V, V_GSS ±20 V, I_D −4.3 A | Drain → `NET_FUSED` (battery side); Source → `+12V_P` (board side); Gate → `NET_QGATE` |
| `R1` | Resistor, 0402 package (a tiny surface-mount size, roughly 1×0.5 mm) | 10 kΩ 1% | `NET_QGATE` → `GND` |
| `D5` | Zener diode (a diode deliberately operated in reverse breakdown to hold a voltage at a fixed level, unlike a normal diode which just blocks reverse current), SOD-323 | 12 V, 500 mW | Cathode → `NET_QGATE`; Anode → `GND` |
| `D1` | **SMBJ33A**, a TVS diode (transient voltage suppressor — a diode built specifically to clamp voltage spikes without being destroyed by them) in an SMB package | 33 V standoff (the voltage it sits below without conducting), clamps to 53.3 V at 11.26 A | Cathode → `+12V_P`; Anode → `GND` |
| `C5` | Ceramic capacitor, 0603 | **100 nF, 100 V**, X7R (a ceramic dielectric type chosen for stable capacitance over temperature and voltage). **100 V, not 50 V** — `D1` clamps this rail at 53.3 V, so a 50 V part is under-rated against the exact event the TVS exists to handle (defect 8.10) | `+12V_P` → `GND` |

**Q1 is doing reverse-polarity protection, and its orientation is not optional.** Q1's protection
comes from its body diode — the parasitic diode every MOSFET has as a side effect of how it is built.
In a P-channel FET that diode conducts DRAIN to SOURCE. So the drain must face the incoming supply
(the battery side) and the source must face the board. Wired that way, normal power flows through the
forward-biased body diode, the source rises to near +12 V while the gate is held at ground, V_GS goes
to about −12 V, and the channel turns hard on and shorts out the diode's voltage drop. On a reversed
supply the body diode is reverse-biased *and* V_GS collapses to about 0 V, so both paths are off and
nothing reaches the board.

Wired the other way round (source to the battery), the board still works perfectly with correct
polarity — which is exactly what makes the error dangerous. On a reversed supply the body diode
becomes forward-biased and dumps the fault straight into the board. You cannot catch this by testing;
it stays invisible until the day someone connects a battery backwards, which is the one day the part
exists for.

**Why there's a zener diode (D5) on the gate of Q1.** With Q1's gate pulled to ground through R1,
the voltage across the gate (V_GS) equals minus whatever is on the input (−V_IN). During a load
dump that's been clamped by the TVS at 53 V, that would put −53 V on a gate only rated for ±20 V —
enough to destroy Q1's gate insulation. D5 holds the gate at −12 V instead, so Q1 survives the same
event it's there to protect against. At the moment the clamp is active, the current through the
zener works out to (53 − 12) / 10 kΩ = 4.1 mA, which is trivial for a 500 mW part.

### 2.2 U1 — the 12 V→5 V buck converter (TI LMR36015)

**What it does and why it exists.** `U1` is a buck converter, a switching circuit that steps a
higher voltage down to a lower one efficiently (unlike a simple resistor divider or a linear
regulator, which just burns off the difference as heat). Turning 12 V into 5 V at the wheel's
current draw with a plain linear regulator would waste roughly 4 W as heat in a part you're holding
in your hands — the buck converter wastes only a fraction of a watt doing the same job, because it
works by rapidly switching the input on and off and averaging the result with an inductor, rather
than resistively dropping the voltage. `U1` is the **TI LMR36015**, in a VQFN-HR-12 package (part
suffix "RNX"). This is the single point on the board that ever sees the full raw automotive voltage
range — everything downstream of it lives in a much gentler 5 V/3.3 V world.

**Pinout** (from the datasheet): pins 1 and 11 are `PGND` (power ground), 2 and 10 are `VIN`, 3 is
`NC` (not connected, per the datasheet's own pin description), 4 is `BOOT`, 5 is `VCC`, 6 is `AGND`
(analog ground), 7 is `FB` (feedback), 8 is `PG` (power good), 9 is `EN` (enable), 12 is `SW`
(switching node).

| Ref | Value | Connection |
|---|---|---|
| `U1.2`, `U1.10` | — | `+12V_P` |
| `U1.1`, `U1.11`, `U1.6` | — | `GND` (both PGND pins and AGND all go to GND) |
| `U1.12` (SW) | — | `NET_SW` |
| `U1.3` (NC) | — | tie to `NET_SW` — the datasheet specifically calls for this so the boost capacitor (`C_BOOT`) routes cleanly |
| `U1.9` (EN) | — | `+12V_P` (the converter is always enabled — there's no separate on/off control). A resistor divider here would give a programmable start-up threshold, which this board does not need: the LMR36015 has its own internal undervoltage lockout and nothing needs sequencing. The datasheet's pin table says of EN, in as many words, *"Can be connected directly to VIN; Do not float"* — so this is its sanctioned arrangement, not a shortcut. |
| `U1.8` (PG) | — | leave open, or add `R_PG` (100 kΩ) to `+3V3` if you want to sense "power good" in firmware |
| `C_BOOT` | 100 nF, 25 V, X7R, 0402 | `U1.4` (BOOT) → `NET_SW` |
| `C_VCC` | 1 µF, 16 V, X7R, 0603 | `U1.5` (VCC) → `GND`. **Do not load VCC externally** — it's an internal regulator output for the chip's own use, not a rail to power other things from |
| `L1` | 10 µH inductor, saturation current (I_sat, the current above which an inductor stops behaving like an inductor) ≥ 2 A, shielded | `NET_SW` → `+5V` |
| `C1`, `C2` | **4.7 µF, 100 V**, X7R, 1206 (×1) plus **220 nF, 100 V**, 0402 (×2) | `+12V_P` → `GND`, one 220 nF at **each** of `U1.2`/`U1.10`. **100 V, not 50 V:** `D1` clamps this rail at 53.3 V, so a 50 V part is under-rated against the event the TVS exists to survive — and the datasheet asks for "at least the maximum input voltage, preferably twice", naming 100 V for the 220 nF parts explicitly (defect 8.10) |
| `C3`, `C4`, `C_O3` | 3 × 15 µF, 16 V, X7R, 0805 | `+5V` → `GND` |
| `R_FBT` | 100 kΩ, 1%, 0402 | `+5V` → `NET_FB` |
| `R_FBB` | 24.9 kΩ, 1%, 0402 | `NET_FB` → `GND` |
| `C_FF` | 20 pF, 0402 | across `R_FBT` (i.e. `+5V` → `NET_FB`) — a feed-forward capacitor that improves the converter's transient response. 20 pF is TI's tabulated value for this exact divider pair (100 kΩ / 24.9 kΩ) — not a value to re-derive or round. If you change `R_FBT` or `R_FBB`, go back to the datasheet table rather than keeping 20 pF. |
| `U1.7` (FB) | — | `NET_FB`. **Never float or ground FB** — the feedback pin is how the chip senses its own output voltage to regulate it; grounding or floating it will send the output voltage to the wrong place (potentially destructively high) |

**Five of these parts are not optional trim — TI's datasheet marks them required, and it's worth
spelling out why, because a part that only exists as a note in a "comments" cell is exactly the kind
of thing a distracted BOM pass drops:**

| Part | Value | What it actually does |
|---|---|---|
| `C_BOOT` | 100 nF | Bootstrap capacitor between `BOOT` and `SW`. The high-side switch inside the chip needs a gate voltage *above* the input voltage to turn on — `C_BOOT` is the small charge reservoir that supplies that boosted voltage. Without it the converter cannot switch at all — not "inefficient," not "noisy," dead. |
| `C_VCC` | 1 µF | Output capacitor for the chip's own small internal regulator, which powers its internal control circuitry. Don't connect anything else to this pin — it isn't a rail to draw current from, only a place for that internal regulator's own output. |
| `R_FBT` / `R_FBB` | 100 kΩ / 24.9 kΩ | The feedback divider — this is how the chip is *told* what output voltage to produce. Without it, `FB` floats and there is no output setpoint at all. |
| `C_FF` | 20 pF | Feed-forward capacitor across `R_FBT` that improves stability. This is TI's tabulated value for this exact resistor pair (100 kΩ / 24.9 kΩ) — if `R_FBT`/`R_FBB` ever change, go back to the datasheet table rather than keeping 20 pF. |

The fifth required part is already in the `C1`/`C2` row above: the two 220 nF capacitors at the
input. The datasheet says these "must" be fitted, one at each VIN/PGND pin pair — they're a
high-frequency bypass for the chip's own internal control circuitry, a separate job from (and not a
duplicate of) the bulk 4.7 µF capacitor that absorbs the larger switching current.

These component values come from TI's own reference table (Table 10-1) for the 1 MHz switching
frequency variant at 5 V output. **This is marked [OPEN — trivial] in the source:** confirm which
switching-frequency variant you actually buy — if it turns out to be the 400 kHz variant instead of
1 MHz, use `L1 = 15 µH` and `COUT = 3 × 22 µF` instead of the values above. Also prefer the
non-PFM ("FPWM", forced pulse-width modulation — a mode that keeps the switching frequency constant
even at light load) variant, because a constant switching frequency is easier to filter out and
causes less erratic RF interference than a variant that skips pulses at light load.

### 2.3 U2 — the 5 V→3.3 V regulator (AP2112K-3.3TRG1)

**What it does and why it exists.** Once you have a clean 5 V, stepping it down further to 3.3 V for
the MCU and display is a small enough voltage drop, at a small enough current, that a linear
regulator (LDO — "low dropout," meaning it can regulate even when the input voltage is only a
little above the output) is fine here; you don't need a second buck converter for this hop. `U2` is
the **AP2112K-3.3TRG1**, SOT-25 package, LCSC part C51118.

| Ref | Value | Connection |
|---|---|---|
| `U2` VIN | — | `+5V` |
| `U2` EN | — | tied to `+5V` (always on) |
| `U2` VOUT | — | `+3V3` |
| `U2` GND | — | `GND` |
| `C6` | 1 µF, 16 V, X7R, 0603 | `+5V` → `GND` at the input pin |
| `C7` | 1 µF, 16 V, X7R, 0603 | `+3V3` → `GND` at the output pin |
| `C7b` | 100 nF, 0402 | `+3V3` → `GND`, placed as close to the output pin as possible |

**Ceramic capacitors are correct here, and that's not a small detail.** The AP2112K is specifically
designed and specified to be stable with ceramic (X7R/X5R) input and output capacitors. This part
was chosen precisely *because* an earlier candidate, the AMS1117, requires a tantalum output
capacitor for stability — pairing that older part with ceramic caps (which have very low ESR,
equivalent series resistance — essentially the tiny bit of built-in resistance a capacitor has) can
make its control loop unstable and put oscillation/ripple on the 3.3 V rail. See
`datasheet-verification.md` §5C for the full story of why that swap happened. The takeaway for you:
don't substitute a different 3.3 V regulator here without checking whether it, too, has an
opinion about capacitor type.

### 2.4 The analog rail and rail-voltage monitors

**What it does and why it exists.** The microcontroller's ADC needs a clean supply to measure
accurately — if you feed it the same 3.3 V that's also switching an LED chain and a buck converter
next door, the digital noise shows up as error in every analog reading. `FB1`, a ferrite bead (a
component that looks resistive to high-frequency noise but passes DC through with almost no loss),
filters `+3V3` down into a separate, quieter `+3V3A` net used only for the chip's analog supply and
reference pins.

| Ref | Value | Connection |
|---|---|---|
| `FB1` | Ferrite bead, 600 Ω @ 100 MHz, ≥500 mA, 0603 | `+3V3` → `+3V3A` |
| `C8` | 1 µF, 0603 | `+3V3A` → `GND` |
| `C9` | 100 nF, 0402 | `+3V3A` → `GND`, within 5 mm of the VDDA pin |

The board also measures its own supply voltages, so firmware can report a low battery or a dead
regulator on the dashboard rather than just quietly misbehaving. Each measurement is a resistor
divider (two resistors in series across a voltage, tapping the midpoint to get a smaller, known
fraction of it) feeding an ADC pin, with a small RC filter to smooth the reading:

| Ref | Value | Connection |
|---|---|---|
| `R2` | 47 kΩ, 1%, 0402 | `+12V_P` → `V12_SENSE` |
| `R3` | 10 kΩ, 1%, 0402 | `V12_SENSE` → `GND` |
| `C10` | 100 nF, 0402 | `V12_SENSE` → `GND` |
| `R4` | 10 kΩ, 1%, 0402 | `+5V` → `V5_SENSE` |
| `R5` | 10 kΩ, 1%, 0402 | `V5_SENSE` → `GND` |
| `C11` | 100 nF, 0402 | `V5_SENSE` → `GND` |

The divider math: 12 V through the 47k/10k divider comes out to 12 × 10/57 = 2.11 V, and 5 V through
the equal 10k/10k divider comes out to 5 × ½ = 2.5 V — both safely inside the 3.3 V range the ADC
can read.

### 2.5 Test points

Five bare test points for a multimeter or scope probe: `TP1` on `+12V_P`, `TP2` on `+5V`, `TP3` on
`+3V3`, and `TP4`/`TP5` both on `GND` (two ground points, spaced apart so a scope's spring-loaded
ground clip can reach one near whatever you're probing).

---

## 3. MCU sheet (`wheel-mcu.SchDoc`)

`U4` is the **STM32G474RET6**, in an LQFP-64 package (LCSC part C521608) — the brain of the board.

### 3.1 Power pins and decoupling

All `VDD` pins on the chip go to `+3V3`; all `VSS` pins go to `GND`. The analog supply pins
(`VDDA`, `VREF+`) go to the filtered `+3V3A` rail instead, and their returns (`VSSA`, `VREF−`) go to
`GND`. Six decoupling capacitors (small capacitors placed right next to a chip's power pins to
supply the instantaneous current spikes a digital chip demands, which a regulator sitting far away
on the board can't respond to fast enough), `C16`–`C21`, are 100 nF 0402 each, one per VDD pin,
placed right at the pin. One larger 4.7 µF 16 V X7R 0805 capacitor, `C22`, sits near the MCU as a
bulk reservoir for the whole chip. `VBAT` (the pin that would normally back up the real-time clock
through a coin cell) just ties to `+3V3` — there's no coin cell on this board because the RTC isn't
used.

### 3.2 The HSE crystal — required, and it was missing from the first draft

**What it does and why it exists.** Every microcontroller needs a clock to run its logic and time
its communication. The STM32G474 has a built-in RC oscillator (HSI16) that needs no external parts
at all — but it's not accurate enough for this board, and using it instead of an external crystal
was an actual mistake caught in an earlier draft.

**Why a crystal is mandatory here, not just "nice to have."** The datasheet gives the internal
HSI16 oscillator's accuracy as −1%/+1% over 0…85 °C, and −2%/+1.5% over the full −40…125 °C range
(Table 43). CAN bus communication tolerates roughly ±0.5% of clock error per node in practice (about
±1.58% in the theoretical best case with perfect sample-point placement) — and that error budget is
shared across every node on the bus, not just this one: two nodes each 1% off from nominal are 2%
apart from each other, which is already outside what CAN can tolerate. Running the wheel on HSI16
would produce intermittent CAN error frames and "bus-off" events (a fault state where a CAN
controller gives up talking on the bus entirely) that get *worse* as the car heats up — a classic
"works perfectly on the bench, fails intermittently in the car" bug, and a nasty one to chase
because it presents exactly like a software bug rather than a clock problem.

A crystal rated at 50 ppm (parts per million) accuracy is 0.005% — a hundred times better than CAN
needs — and standard crystals are cheap, so there's no reason to cut this corner.

**USB doesn't force this decision, but rides along for free.** The G4 chip can technically do
USB without any crystal at all, using its HSI48 internal oscillator plus a feature called the Clock
Recovery System that trims the internal oscillator against timing marks in the USB data stream. But
since CAN already requires a crystal, it's simpler to clock *both* CAN and USB from the same crystal
via the chip's PLL (phase-locked loop, a circuit that multiplies a clock up to a higher frequency)
and eliminate the whole question of clock accuracy in one move, rather than juggling two different
clock sources for two peripherals.

**How it's built.** The oscillator pins are `PF0-OSC_IN` (LQFP-64 pin 5) and `PF1-OSC_OUT` (pin 6).
Both are confirmed bonded (i.e. actually connected to a physical pin) on this package.

| Ref | Part / value | Connection |
|---|---|---|
| `Y1` | HSE crystal, 8 MHz or 16 MHz, ≤50 ppm initial + temperature accuracy, load capacitance (CL) 8–12 pF, ESR (equivalent series resistance — a measure of internal loss) ≤80 Ω, SMD 3225 package | pin 1 → `OSC_IN` (PF0), pin 2 → `GND`, pin 3 → `OSC_OUT` (PF1), pin 4 → `GND` |
| `C_X1`, `C_X2` | 2 × (CL − C_stray), C0G dielectric (a very stable, low-loss ceramic type used specifically for timing circuits), 0402 — roughly 18 pF for a 12 pF CL crystal, or roughly 10 pF for an 8 pF CL crystal | `OSC_IN` → `GND`, `OSC_OUT` → `GND` |
| `R_X1` | 0 Ω, 0402 — a footprint reserved for a series damping resistor, not necessarily fitted at that value | in series between `OSC_OUT` and `Y1` pin 3 |

**This is marked [OPEN — pick and verify a part]:** the crystal spec above (8/16 MHz, ≤50 ppm,
CL 8–12 pF) is *derived* from what CAN needs, not copied from one specific datasheet. Once you pick
an actual crystal part, recompute `C_X1`/`C_X2` from **that part's own CL** using
`C = 2 × (CL − C_stray)`, with C_stray (stray capacitance from the PCB traces and pads) estimated at
roughly 3–5 pF for this layout geometry. Do not just carry over the 18 pF figure above without
redoing this calculation for the crystal you actually bought.

### 3.3 Reset, boot, and debug

| Ref | Value | Connection |
|---|---|---|
| `C23` | 100 nF, 0402 | `NRST` (the chip's reset pin) → `GND` |
| — | — | `NRST` also connects to the Tag-Connect debug header |

**BOOT0 gets no component at all — this is a warning, not an oversight.** `PB8-BOOT0` is the pin the
chip samples at power-up to decide whether to run your program or drop into the built-in bootloader
— but on this chip's pinout, `PB8` is *also* `FDCAN1_RX`, the CAN receive line. Do not fit a pulldown
resistor here. A pulldown would fight against the CAN transceiver's own output driving that same
pin, which is actively harmful, not just redundant. Instead, BOOT0's behavior comes from the chip's
`nBOOT0` option bit (set `nSWBOOT0 = 0` so BOOT0 comes from the option bit, and `nBOOT0 = 1` to boot main flash) — a setting stored in the chip's non-volatile
configuration memory, not a voltage on a pin. This must be set the first time the board is flashed,
and **re-checked after any full chip erase**, because a mass erase can restore the factory default
and silently undo it. (See `datasheet-verification.md` §1 defect 1.3 for the full failure story: an
idle CAN bus sits at the "recessive" logic level, which is HIGH — so if BOOT0 were ever read from the
pin instead of the option bit, a board connected to a live CAN bus at power-on would sample BOOT0=1
and jump straight to the bootloader instead of running firmware. It would work perfectly on the
bench with the bus disconnected and simply never start in the car.)

| Ref | Part | Connections |
|---|---|---|
| `J_SWD` | Tag-Connect **TC2030-CTX** footprint (just copper pads and 3 locating holes — no physical connector part to buy) | pin 1 `+3V3`, 2 `SWDIO` (PA13), 3 `NRST`, 4 `SWCLK` (PA14), 5 `GND`, 6 NC |
| `J4` | JST-GH 3-pin, `SM03B-GHS-TB` | 1 `DBG_TX` (PA9), 2 `DBG_RX` (PA10), 3 `GND` |

⚠ **`PA9`/`PA10` have a second job that bites the moment this connector is used — see §3.5, the UCPD
dead-battery trap, before relying on the debug UART to diagnose anything.**

### 3.4 USB-C (for the simulator variant and DFU firmware updates)

**What it does and why it exists.** The same board design can run in two contexts: bolted into the
race car, or plugged into a desktop sim rig over USB. This section wires up a USB-C connector so the
sim variant can be powered and programmed over USB, without changing anything else on the board.

| Ref | Part / value | Connections |
|---|---|---|
| `J2` | USB-C receptacle, 16-pin, LCSC `C165948` | see pin notes below |
| `U5` | **USBLC6-2SC6**, an ESD protection chip (a device that clamps electrostatic discharge spikes on data lines without disturbing the signal) in a SOT-23-6 package, LCSC `C7519` | I/O1 ↔ `USB_DM_CON`, I/O2 ↔ `USB_DP_CON`, VBUS pin → `NET_VBUS`, GND → `GND`; the protected (chip) side connects to `USB_DM`/`USB_DP` |
| `R9`, `R10` | 5.1 kΩ, 1%, 0402 | `CC1` → `GND`, `CC2` → `GND` (this tells the USB-C host "I'm a UFP" — upstream-facing port, i.e. a peripheral — requesting the 500 mA default) |
| `D6` | **BAT60A** Schottky diode, SOD-123 | Anode on `NET_VBUS` → Cathode on `NET_VBUS_OR` |
| `R_VBUS` | 0 Ω, 0603 — **DNP** ("do not populate": place the footprint on the board but don't solder the part) **in the car build, FIT in the sim build** | `NET_VBUS_OR` → `+5V` |
| `R11` | 1 MΩ, 0402 | connector shield → `GND` |
| `C24` | 4.7 nF, 0402 | connector shield → `GND` (in parallel with R11) |

The connector's D+/D− pins (both orientation pairs, since USB-C is reversible) tie together to
`USB_DM_CON`/`USB_DP_CON`; its VBUS pins go to `NET_VBUS`; ground and shield follow the connector's
own datasheet pinout.

**The USB power feeds `+5V`, downstream of the buck converter — it must never feed `+12V_P`.** This
is the point of the diode-OR arrangement (`D6` plus `R_VBUS`): when the sim variant is powered from
USB with the buck converter unpopulated, USB's 5 V steps in to supply the board's `+5V` rail
directly. Wiring USB power upstream of the buck instead would mean USB's 5 V has to fight or bypass
a converter designed to take 12 V, which is not what it's for.

### 3.5 The UCPD dead-battery trap — a firmware requirement hiding in the pin map

**What it does and why it exists.** `PA9` and `PA10` do double duty on this chip. §3.3 already uses
them as `DBG_TX`/`DBG_RX`, the debug UART — but they are also `UCPD1_DBCC1` and `UCPD1_DBCC2`, the
"dead battery" sense inputs for USB Type-C Power Delivery. USB-C Power Delivery has a rule built into
the spec: a device with a flat battery must still present a resistor on its CC pins, or a charger has
no way to see it and offer power. ST wired that requirement directly into the silicon — whenever a
DBCC pin reads HIGH, the chip switches a 5.1 kΩ pull-down resistor onto the matching CC pin, in
hardware, on its own, **without the UCPD peripheral being enabled, configured, or used by firmware in
any way.**

**This board doesn't use USB Power Delivery at all. That doesn't help.** The pull-down is armed by
*pin voltage*, not by software choosing to switch a peripheral on. `PA9` is the debug UART's transmit
line, and an idle serial line sits HIGH. So the moment firmware switches on the debug UART, the chip
arms a 5.1 kΩ pull-down on `PB6` — which, on this board, is `ENC3_A`, one half of encoder 3.

`PB6`'s own conditioning cell (§5.1) already carries a 10 kΩ pull-up to `+3V3`. Put a 5.1 kΩ
pull-down into that fight, and the idle voltage settles at 3.3 × 5.1/(10 + 5.1) = **1.11 V**. The pin
needs to see at least 2.31 V (0.7 × `+3V3`) to register as a logic HIGH. It never gets there, so
`ENC3_A` can never read high, and the timer decoding encoder 3 (TIM4, §9) stops counting.

**The symptom is deliberately cruel.** Encoder 3 works perfectly in a normal build with the debug
UART untouched. It breaks the instant you plug in a debug cable to find out why something else is
wrong — it fails only while you are watching it, which is close to the worst possible way for a bug
to behave.

**The fix is two lines of firmware, and they must run before any pin is configured:**

```c
RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
PWR->CR3      |= PWR_CR3_UCPD1_DBDIS;   // release the dead-battery pull-downs on PB4/PB6
```

Treat this exactly the way §3.3 treats the `nSWBOOT0`/`nBOOT0` option bits: it's a provisioning
setting, not a schematic connection, and it has to be right or the board misbehaves in a way that
looks nothing like its actual cause.

**There is no hardware fix.** Moving `ENC3_A` off `PB6` isn't an option — every timer pin pair on
this package that can decode a quadrature encoder is already spoken for by ENC1–ENC6 (§9).
Strengthening the pull-up to overpower the 5.1 kΩ pull-down doesn't work either: drop it to
2.2 kΩ and the idle voltage lands at 3.3 × 5.1/(2.2 + 5.1) ≈ 2.31 V — dead on the logic-high
threshold, with zero margin — and anything smaller than that breaks the pin's logic-LOW level
instead, once the switch actually closes. The firmware bit is the only fix, and it has to run
unconditionally, not only when a debugger happens to be attached.

**Bring-up gate:** with a debug cable plugged in, confirm encoder 3 counts in both directions.

---

## 4. CAN and I/O sheet (`wheel-can-io.SchDoc`)

### 4.1 U3 — the CAN transceiver (TJA1051T/3)

**What it does and why it exists.** The microcontroller's built-in CAN peripheral only produces
simple logic-level signals (TXD/RXD). To actually drive the two-wire differential CAN bus that runs
the length of the car, you need a transceiver chip that converts those logic signals into the
CAN_H/CAN_L differential pair (and back). `U3` is the **TJA1051T/3**, SO-8 package. The `/3` variant
matters specifically because it has a separate `VIO` pin that lets its logic-side pins run at 3.3 V
(matching the MCU) while the bus-side electronics run at the full 5 V CAN needs for good noise
margin on a car harness — no extra level-shifting parts required.

| Pin | Name | Connection |
|---|---|---|
| 1 | TXD | `CAN_TX` (PB9) |
| 2 | GND | `GND` |
| 3 | VCC | `+5V` |
| 4 | RXD | `CAN_RX` (PB8) |
| 5 | VIO | `+3V3` |
| 6 | CANL | `CAN_L` |
| 7 | CANH | `CAN_H` |
| 8 | S | `GND` (silent mode off = normal operating mode) |

`C12`, 100 nF 0402, decouples pin 3 (VCC); `C13`, 100 nF 0402, decouples pin 5 (VIO).

### 4.2 Bus protection and termination

**What it does and why it exists.** The CAN bus runs the length of the car's harness, so it's
exposed to electrical noise and transients from the whole vehicle. `D2` clamps those before they
reach the transceiver. Separately, a correctly-terminated CAN bus needs exactly two 120 Ω-equivalent
terminating resistors, one at each physical end of the bus — this board provides the footprint for
one, but does not populate it by default, because whether this particular board sits at a physical
end of the bus depends on the harness design, not on the board itself.

| Ref | Part / value | Connection | Fit |
|---|---|---|---|
| `D2` | **PESD2CAN**, SOT-23 — 24 V standoff, clamps to 41 V at 5 A | across `CAN_H`/`CAN_L` to `GND` | Fit |
| `R_T1` | 60.4 Ω, 1%, 0805 | `CAN_H` → `NET_TERMMID` | **DNP** |
| `R_T2` | 60.4 Ω, 1%, 0805 | `NET_TERMMID` → `CAN_L` | **DNP** |
| `C_T1` | 4.7 nF, 50 V, 0603 | `NET_TERMMID` → `GND` | **DNP** |

**Add a bold schematic note: populate the termination only if this board is a physical end of the
bus.** Populating it on a board in the middle of the bus, or failing to populate it on a board that
genuinely is the end, both break CAN bus signal integrity. `D2`'s 41 V clamp is comfortably under
the TJA1051's ±58 V bus fault tolerance, so it protects the transceiver without interfering with
normal bus operation.

### 4.3 Paddle pass-through and sense

**What it does and why it exists.** The shift paddles are not read by this board's microcontroller
at all — they're wired straight through to the car's ECU (engine control unit), which is what
actually needs to know when a paddle is pulled. The wheel board just passes the signal through
copper and additionally taps it (without loading or disturbing it) so the wheel's own display can
show shift events too.

For each line (UP and DOWN):

| Ref | Value | Connection |
|---|---|---|
| `D3` / `D4` | **SMAJ24CA**, a bidirectional TVS diode, SMA package | `PADDLE_UP` → `GND` / `PADDLE_DOWN` → `GND`, placed at the connector |
| `R6` / `R7` | 150 kΩ, 1%, 0402 | `PADDLE_UP` → `PADDLE_UP_SNS` / `PADDLE_DOWN` → `PADDLE_DN_SNS` |
| `R6b` / `R7b` | 39 kΩ, 1%, 0402 | `PADDLE_UP_SNS` → `GND` / `PADDLE_DN_SNS` → `GND` — the lower half of the divider. Without it, the pin sees the full paddle-line voltage instead of a safe fraction of it. |
| `C14` / `C15` | 1 nF, 0402 | each `*_SNS` net → `GND`, at the MCU pin |
| `D14` / `D15` | **BAV199**, a dual low-leakage silicon diode, SOT-23 package | anode → `PADDLE_UP_SNS` / `PADDLE_DN_SNS`, cathode → `+3V3` |

`PADDLE_UP` and `PADDLE_DOWN` run as plain copper from `J1` pins 4 and 5 straight out to wherever
they're going — nothing on this board switches or buffers them. `R6`/`R7` and `R6b`/`R7b` together
are a high-value observation divider, sized so the current they can ever draw stays far too small to
influence or damage the ECU circuit on the other end.

**Why D14 and D15 exist — the connector protection diode alone isn't enough.** `D3`/`D4` clamp a
transient spike on the paddle lines before it reaches anything else, and they do that job correctly,
clamping at 38.9 V. The problem is what happens *downstream* of that clamp: 38.9 V through the
150 kΩ/39 kΩ divider still puts 38.9 × 39/189 = **8.03 V** on the MCU pin — and that pin is rated for
a 4.0 V absolute maximum. The existing protection was letting through roughly double what the pin can
survive.

The chip can't rescue itself here either. These pins (the datasheet's `TT_a` class) have no internal
protection diode routing excess voltage back to the 3.3 V rail — the datasheet states plainly that
"positive current injection" (current flowing into the pin when its voltage rises above the chip's
own supply, which a protection diode would normally shunt safely away) is "not possible" on them.
That phrase reads as reassuring on a first pass; it actually means the mechanism that would have
saved the pin during the TVS clamp event isn't there at all. The absence of a limit is the warning,
not the all-clear.

`D14`/`D15` supply the missing clamp directly at the pin: each one holds its sense net to about 3.9 V,
comfortably under the 4.0 V limit, while the 150 kΩ upper leg of the divider limits the current
through the diode to (38.9 − 3.9) / 150 kΩ = **0.23 mA** — nothing for a part rated for this.

**Why BAV199 and not a Schottky diode.** On a high-impedance sensing node like this one, a diode's own
leakage current doesn't stay contained — it flows into the same net you're trying to measure and
becomes error in the reading. A Schottky diode leaks considerably more than a low-leakage silicon
part like the BAV199, which is exactly the wrong trade on a node this high-impedance. This is the same
reasoning already applied to the dash's analog inputs (see `datasheet-verification.md` defect 5.2).

---

## 5. Human-interface sheet (`wheel-hmi.SchDoc`)

### 5.1 The standard conditioning cell — the same little circuit, repeated 24 times

**What it does and why it exists.** Every single switch contact on the board — every encoder's A
and B lines and push switch, every button — is a piece of metal that a driver's glove or thumb
touches, mechanically bounces when actuated, and (for lines that leave the board) can pick up
electrical noise from the harness. Feeding that straight into a microcontroller pin is asking for
trouble: contact bounce (a switch mechanically "chattering" for a brief moment as it closes, which a
fast digital input reads as many rapid clicks instead of one) and electrical noise both need to be
dealt with before the signal reaches the chip. Rather than design a bespoke circuit for each input,
the same small circuit, called the "conditioning cell," is repeated identically for every encoder A,
B, and SW line and every button line — 24 times total.

```
switch/encoder contact ──┬── R(1 kΩ) ──┬── MCU pin
                         │             ├── C(100 nF) ── GND
                        GND            └── R(10 kΩ) ── +3V3
```

- The series 1 kΩ resistor limits any injected current into the microcontroller pin to under 5 mA,
  even in the worst case of a direct short to the 5 V rail.
- The 10 kΩ pull-up resistor (a resistor that holds a pin at a defined "high" voltage until
  something actively pulls it low) is deliberately **external** to the chip, not the microcontroller's
  own internal pull-up, so the line has a known, defined state even while the microcontroller itself
  is in reset and its internal pull-ups aren't active yet.
- The 100 nF capacitor to ground, combined with the resistors, forms an RC filter (a resistor-
  capacitor pair whose combination smooths out fast voltage changes): the falling edge (switch
  closing, pulled to ground through the 1 kΩ) has a time constant (τ, roughly how long the voltage
  takes to settle after a change) of about 100 µs, and the rising edge (switch opening, pulled up
  through the 10 kΩ) has a time constant of about 1 ms. Both are comfortably faster than the roughly
  8 ms gap between edges you'd see even on a briskly spun encoder, so the filtering doesn't limit how
  fast someone can actually turn the knob.

Designators: `R13`–`R36` are the series 1 kΩ resistors, `R37`–`R60` are the 10 kΩ pull-ups, `C51`–`C74`
are the 100 nF capacitors.

### 5.2 Encoders

**What they are and why they're built this way.** An encoder is a rotary knob that reports relative
rotation rather than an absolute position, using two output signals (A and B) offset from each other
in "quadrature" — meaning the two square-wave outputs are offset so that whichever one changes state
*first* tells you which direction the knob is turning, while the number of transitions tells you how
far. This board uses two different encoder parts depending on where they sit:

| Ref | Part | Nets |
|---|---|---|
| `ENC1`–`ENC4` | **PEC09-2120F-S0012** — a right-angle encoder (its shaft points sideways out of the board rather than straight up), 12 pulses per revolution (PPR), 12 detents, with an integrated push switch | A → `ENC1_A`… B → `ENC1_B`… SW → `ENC1_SW`… (and so on through ENC4), commons → `GND` |
| `ENC5`, `ENC6` | **PEC11H-4120F-S0020** — a vertical-bushing encoder (shaft points straight up through a panel), 20 PPR, 20 detents, with an integrated push switch | `ENC5_A/B/SW`, `ENC6_A/B/SW`, commons → `GND` |

The right-angle parts (ENC1–4) are for thumb-operated controls, where the wheel's faceplate is
roughly parallel to the main PCB and a thumb reaches in from the edge; the vertical parts (ENC5–6)
are for faceplate-mounted rotary knobs a driver reaches with their fingers from the front. See
`hardware-selections.md` §4.1–4.2 for the mechanical reasoning behind that split.

**ENC3 carries an extra hazard the others don't.** Its `A` line shares a pin with a USB-C dead-battery
sense function that can silently disable it — see §3.5, the UCPD dead-battery trap.

### 5.3 Buttons

| Ref | Part | Nets |
|---|---|---|
| `SW1`–`SW6` | C&K **KSC4** series, sealed (IP67, meaning dust-tight and protected against water jets) tactile switches | one side → `BTN1`…`BTN6`, other side → `GND` |
| `J5`, `J6` | JST-GH 2-pin, `SM02B-GHS-TB` | pin 1 → `BTN5` / `BTN6` (wired in parallel with SW5/SW6), pin 2 → `GND` |
| `D7`, `D8` | **BAV99**, a dual series diode in one SOT-23 package | clamp `BTN5`/`BTN6` to `+3V3` and `GND` at J5/J6 |

`J5` and `J6` let a button be relocated off the main board onto a remote panel switch on a cable,
wired in parallel with the on-board switch. Because those lines physically leave the board through a
cable, they get the extra BAV99 diode clamp — any line that leaves the board picks up more
electrical noise and ESD risk than one that stays entirely on it.

### 5.4 Satellite encoder provision — all DNP (not populated) on this board

| Ref | Part | Nets |
|---|---|---|
| `J7`–`J10` | JST-GH 5-pin, `SM05B-GHS-TB` | 1 `+3V3`, 2 `ENCn_A`, 3 `ENCn_B`, 4 `ENCn_SW`, 5 `GND` (n = 1…4) |
| `D9`–`D12` | BAV99, SOT-23 | clamp each satellite line to `+3V3`/`GND` |

**What this is for.** These four connectors are footprints only — placed on the board but not fitted
with parts. They exist so that, without redesigning the main board, any of the four thumb encoders
(ENC1–4) can instead be moved onto a small separate "satellite" PCB connected by cable, using the
exact same conditioning cell described in §5.1 but built on the satellite board instead. This is
useful if the thumb encoder's fixed right-angle geometry doesn't end up landing where the driver's
thumb actually reaches — you'd swap in a satellite board rather than respin the whole wheel PCB.

---

## 6. LED sheet (`wheel-leds.SchDoc`)

**What it does and why it exists.** The wheel has 24 addressable RGB LEDs (WS2812B parts, meaning
each one has its own tiny controller chip built in, and all 24 can be individually colored and
chained off a single data wire) arranged as a shift-indicator bar, a traction-control indicator bar,
and a lockup (differential/clutch) indicator bar. Driving 24 individually-colorable LEDs from a
single microcontroller pin is only possible because of that per-LED addressing — a design using
plain LEDs would need a separate current-driver chip and much more wiring for far less flexibility.

| Ref | Part / value | Connection |
|---|---|---|
| `U6` | **74AHCT1G125**, a single logic buffer chip, SOT-353 package | VCC → `+5V`, GND → `GND`, `/OE` (output enable, active low) → `GND`, A (input) → `LED_DATA_3V3` (PA6), Y (output) → `NET_LEDBUF` |
| `C_U6` | 100 nF, 0402 | `+5V` → `GND` at U6 |
| `R12` | 300 Ω, 0402 | `NET_LEDBUF` → `LED_DATA_5V` |
| `LED1`–`LED24` | **WS2812B-2020**, LCSC C965555 | VDD → `+5V`, VSS → `GND`; DIN of LED1 ← `LED_DATA_5V`, then each LED's DOUT feeds the next LED's DIN in a daisy chain through LED24 |
| `C25`–`C48` | 100 nF, 0402 | one per LED, `+5V` → `GND` at each LED |
| `C49`, `C50` | 100 µF, 10 V | one bulk capacitor at the shift bar, one shared by the traction-control/lockup bars |

Chain order: LED1–LED16 form the shift bar (left to right), LED17–LED20 form the traction control
(TC) bar, and LED21–LED24 form the lockup bar.

**Why the buffer (U6) is mandatory, not an optional nicety.** This is a level-shifting problem: the
microcontroller's data pin (`LED_DATA_3V3`, PA6) drives 3.3 V logic, but the WS2812 LEDs are powered
from 5 V and need their data input to reach roughly 0.7 × 5 V = 3.5 V to reliably register a logic
high — a level that 3.3 V logic cannot guarantee to hit directly. `U6`, the 74AHCT1G125, solves this
correctly rather than as a hack: it's a 5 V-powered buffer, but its *input* threshold (the "AHCT"
family specifically uses TTL-compatible input thresholds, V_IH = 2.0 V) is low enough to legally
accept a 3.3 V logic signal as a valid high, while its *output* swings all the way up to the full
5 V rail the LEDs need. A plain "HC" family part (rather than "AHCT") would not work here, because
plain HC parts use CMOS-level input thresholds tied to their own supply voltage rather than the
lower, fixed TTL-style thresholds — at 5 V supply, a plain HC input threshold would sit too high for
3.3 V logic to reliably trigger. **This is not a place to substitute a similar-looking chip.**

---

## 7. Display sheet (`wheel-display.SchDoc`) — colour display

**What it does and why it exists.** `DS1` is the **JDI LPM013M126A**, a 1.28-inch, 176×176 pixel,
8-colour reflective "memory-in-pixel" LCD — a type of display that stores each pixel's state in an
on-glass memory cell and modulates *reflected ambient light* rather than emitting its own light. That
matters enormously for a wheel display: an emissive display (like a normal phone screen) has to
compete with direct sunlight, which can be tens of thousands of times brighter than the display
itself; a reflective display instead gets *more* legible as ambient light increases, because it's
literally reflecting that light back at the viewer. `J3` is the 10-pin, 0.5 mm pitch FPC ZIF
connector (a flexible flat cable connector with a zero-insertion-force latch) that plugs the display
module into the board.

This display's pinout has been verified against the JDI specification and is pin-for-pin identical
to the Sharp mono display it replaces, so swapping to colour was a BOM change only — same footprint,
same net names, same firmware structure.

| Pin | Signal | Connect to |
|---|---|---|
| 1 | SCLK | `LCD_SCLK` (PA5) |
| 2 | SI | `LCD_SI` (PA7) |
| 3 | SCS | `LCD_SCS` (PA4) |
| 4 | EXTCOMIN | `LCD_EXTCOMIN` (PC3) |
| 5 | DISP | `LCD_DISP` (PB13) — high = show the stored memory contents, low = go black, but the memory contents are retained either way |
| 6 | VDDA | `+3V3` |
| 7 | VDD | `+3V3` |
| 8 | **EXTMODE** | `+3V3` via `R_EXTMODE`, a 0 Ω link (per JDI: "H = enable EXTCOMIN, connect to VDD"); the alternative, `R_EXTMODE_L` (0 Ω to `GND`), is on the footprint but **DNP** |
| 9 | VSS | `GND` |
| 10 | VSSA | `GND` |

`C75` (100 nF) and `C76` (1 µF) sit at the connector, from `+3V3` to `GND`.

### Three rules this display imposes — none of them are optional

1. **VDD and VDDA both come from the microcontroller's own `+3V3` rail, not a separate supply.** The
   datasheet requires a logic high to be within 100 mV of the display's own VDD (its spec is
   `V_IH = VDD − 0.1 V`). Sharing the exact same rail as the microcontroller means the microcontroller's
   own output-high voltage naturally tracks the display's threshold as both rails sag or shift
   together. Powering the display from a separate or lower 3.3 V source breaks this relationship and
   risks the display simply not registering the microcontroller's signals as valid.
2. **VDDA must never exceed VDD** (the datasheet's stated maximum for VDDA is "VDD"). Both pins tie
   to the same `+3V3` net for exactly this reason — do not "improve" this by giving VDDA its own
   separately filtered or higher-voltage rail.
3. **EXTMODE must not float, exactly as on the Sharp part it replaced.** Tying EXTMODE high selects
   the hardware EXTCOMIN path that this design uses to invert the display's internal "COM" signal —
   a signal that must periodically flip polarity to prevent a DC bias from building up across the
   liquid crystal material. ⚠ **If EXTMODE is left floating, COM inversion becomes undefined, DC bias
   can build up across the liquid crystal, and this permanently damages the display panel.** This is
   not a soft failure mode — it is physical damage to the glass.

### Firmware constraints that don't show up on the schematic — don't lose these

- **The SPI clock (SCLK) has a hard maximum of 2.00 MHz** (1.00 MHz is typical). The STM32's SPI1
  peripheral will happily run far faster than that if firmware doesn't explicitly cap it — capping
  the clock rate is a firmware responsibility with no schematic safety net.
- **`LCD_EXTCOMIN` must be toggled at roughly 1 Hz continuously**, for as long as the panel is
  powered — this is the software half of the same COM-inversion mechanism EXTMODE enables in
  hardware.

### Ratings and risks

Power draw is a trivial 115.5 µW maximum — a rounding error in the board's power budget. Absolute
maximum VDD is 3.6 V. ⚠ **Operating temperature is only −20…+70 °C, narrower than the rest of the
wheel's parts list**, and a black steering wheel sitting in direct sun could plausibly exceed that at
the panel surface — this is recorded as open assumption A9, to be checked by measuring an actual
wheel on a summer track day.

**A zero-board-change fallback exists.** If colour stock becomes unavailable, the **Sharp
LS013B7DH05** (144×168, monochrome, LCSC `C17500193`, JLC-assemblable) is pin-for-pin identical and
can be fitted with no schematic or layout change — keep both parts in the library. The tradeoff going
the other way (choosing colour) is sourcing: the JDI part comes from specialty display distributors
(Switch-Science, Data Modul, Youritech) rather than Digi-Key/LCSC, which is the one thing that's
worse about the colour option.

---

## 8. Connector J1 and the top sheet

`J1` is a Deutsch **DTM04-6P** (the panel-mount half; the mating harness side is DTM06-6S), the
single connector through which the whole wheel talks to the rest of the car:

| Pin | Net |
|---|---|
| 1 | `+12V_IN` |
| 2 | `CAN_H` |
| 3 | `CAN_L` |
| 4 | `PADDLE_UP` |
| 5 | `PADDLE_DOWN` |
| 6 | `GND` |

On the top sheet: place the seven sheet symbols (one per schematic sheet described above), wire the
ports between them, then run **Project ▸ Validate** in Altium. The bar to clear before moving on to
layout is **zero errors and zero unsuppressed warnings.**

---

## 9. The complete MCU pin assignment

This table is the master list of which physical microcontroller pin does what — it has been checked
against the STM32G474 datasheet's alternate-function table (Table 13) to make sure each assignment
is actually valid, not just plausible-looking.

| Pin | Net | Peripheral justification |
|---|---|---|
| PC0 / PC1 | `ENC1_A` / `ENC1_B` | TIM1_CH1 / TIM1_CH2 |
| PC6 / PC7 | `ENC2_A` / `ENC2_B` | TIM3_CH1 / TIM3_CH2 |
| PB6 / PB7 | `ENC3_A` / `ENC3_B` | TIM4_CH1 / TIM4_CH2. ⚠ **`PB6` is also `UCPD1_CC1`** — firmware must set `PWR_CR3.UCPD1_DBDIS`, or a 5.1 kΩ dead-battery pull-down kills this input. See §3.5 |
| PA15 / PB3 | `ENC4_A` / `ENC4_B` | TIM2_CH1 / TIM2_CH2 |
| **PB2 / PC2** | `ENC5_A` / `ENC5_B` | **TIM20_CH1 / TIM20_CH2** (TIM15 cannot decode encoders — defect 1.5) |
| PA0 / PC12 | `ENC6_A` / `ENC6_B` | TIM5_CH1 / TIM5_CH2 |
| PC4, PC5, PC8, PC9, PC10, PC11 | `ENC1_SW`…`ENC6_SW` | GPIO input |
| PC13, PD2, PA3, PB10, PB11, PB12 | `BTN1`…`BTN6` | GPIO input |
| PA6 | `LED_DATA_3V3` | TIM16_CH1 + DMA |
| PA5 / PA7 / PA4 | `LCD_SCLK` / `LCD_SI` / `LCD_SCS` | SPI1_SCK / SPI1_MOSI / GPIO |
| PB13 / PC3 | `LCD_DISP` / `LCD_EXTCOMIN` | GPIO (EXTCOMIN is a roughly 1 Hz software toggle) |
| PB8 / PB9 | `CAN_RX` / `CAN_TX` | FDCAN1 — the only option once USB claims PA11/PA12 |
| PA11 / PA12 | `USB_DM` / `USB_DP` | USB FS |
| PA13 / PA14 | `SWDIO` / `SWCLK` | debug |
| PA9 / PA10 | `DBG_TX` / `DBG_RX` | USART1. ⚠ **Also `UCPD1_DBCC1` / `UCPD1_DBCC2`** — a high level here arms the dead-battery pull-down on `PB6` (and `PB4`). See §3.5 |
| PA1 / PA2 | `V12_SENSE` / `V5_SENSE` | **ADC12_IN2 / ADC1_IN3** |
| PB0 / PB1 | `PADDLE_UP_SNS` / `PADDLE_DN_SNS` | **ADC1_IN15 / ADC1_IN12 — read as ADC, not GPIO** (§4.3) |
| PF0 / PF1 | `OSC_IN` / `OSC_OUT` | HSE crystal — LQFP-64 pins 5 and 6, mandatory for reliable 1 Mbit/s CAN (§3.2 above) |
| PA8, PB4, PB5, **PB14, PB15** | spare — PB14/PB15 were freed when `ENC5` moved off TIM15 (defect 1.5) | bring to test points if convenient. ⚠ If you ever use **PB4**, note it is `UCPD1_CC2` and carries the same dead-battery pull-down described in §3.5 |

**Every encoder pair uses channels 1 and 2 (CH1/CH2) of a single timer, and that's not a
coincidence — it's a hard requirement.** The STM32's hardware "encoder mode," which does quadrature
decoding entirely in hardware (freeing the software from having to catch every single edge in an
interrupt), only works across CH1 and CH2 of the *same* timer. Do not substitute different pins here
without re-checking that the replacement pair is still CH1+CH2 of one timer — four of the six
encoder pairs were assigned to invalid pin pairs in an earlier draft for exactly this reason (see
`datasheet-verification.md` §1, defect 1.1), and the failure mode is subtle: it configures without
obvious error, but the encoder simply doesn't count correctly, or at all.

**A pin's ADC channel number is a different question from which pin it is — and the rail monitors
above got caught by exactly that gap.** The STM32G474's ADC channels are numbered independently of
the GPIO numbers they land on. Per DS12288 Table 12: `PA0` is `ADC12_IN1`, `PA1` is `ADC12_IN2`,
`PA2` is `ADC1_IN3`, and `PA3` is `ADC1_IN4` — the channel count is offset by one from the pin count,
in both directions, so a quick "PA1 must be channel 1" check looks self-consistent and is wrong.
Firmware originally configured channel 3 and channel 4 for `V12_SENSE`/`V5_SENSE`, on the assumption
that those channel numbers reached `PA1`/`PA2`. They didn't: channel 3 is actually `PA2`, which is
`V5_SENSE`, and channel 4 is actually `PA3`, which on this board is `BTN3`.

Both resulting failures were quiet ones. The "12 V" reading became the 5 V divider's 2.5 V multiplied
by the 12 V channel's own 5.7× ratio — 2.5 × 5.7 = **14.25 V**, a thoroughly plausible
charging-system voltage that never looks wrong on a dashboard. The "5 V" reading became `BTN3`: idle
high through its pull-up reads 3.3 × 2 = **6.6 V** (a plausible 5 V rail), dropping to **0 V** the
instant someone presses button 3 — a phantom rail collapse timed to a button press. Neither number
looks broken, which is exactly what makes this class of bug dangerous: "is this pin connected to an
ADC?" and "which channel number is this pin?" are different questions, and the first one passing
feels exactly like the second one passing. (See `datasheet-verification.md` defect 8.3.)

---

## 10. What's still open

These items don't block starting the schematic, but need to be closed before parts are actually
ordered (the "G6 pre-order gate"):

- **LMR36015 switching-frequency variant.** ⚠ **Order the `LMR36015FBRNXR`** — the 1 MHz, forced-PWM part, which is what every passive value in §2.2 assumes. **The variant LCSC stocks is the `LMR36015AQRNXRQ1`, which is 400 kHz and non-FPWM**, so if you buy from stock without checking you get the wrong part: it needs a 15 µH inductor and 3 × 22 µF of output capacitance instead of 10 µH and 3 × 15 µF. The automotive qualification on the AQ part is a real benefit — just make it a decision rather than an accident of what was in the cart.
  instead of the 1 MHz-table values used in §2.2).
- **Display FPC contact side (top vs. bottom).** Not stated in the extracted display spec, and a
  top-contact FPC connector would mirror the pinout — check the mechanical drawing before selecting
  the `J3` footprint.
- **HSE crystal part number and load capacitors.** The spec (§3.2) is derived from CAN's timing
  requirements, not copied from one datasheet — pick an actual part, then recompute `C_X1`/`C_X2`
  from that part's own CL.
- **Exact WS2812B-2020 current draw.** The manufacturer's datasheet is image-only (no extractable
  text), so this needs a bench measurement rather than a datasheet read. The firmware's 0.45 A
  aggregate cap holds regardless of the exact per-LED figure.
- **SMAJ5.0A / SMBJ5.0A exact clamp parameters.** Not yet read from a datasheet; judged low risk
  because their standoff voltage clearly exceeds the rails they sit on, but worth a quick check.
  (`SMAJ24CA`'s clamp voltage is now confirmed at 38.9 V — see §4.3 — and it's exactly what drove the
  addition of `D14`/`D15`.)

---

## Things I could not explain simply

None remaining — the two items previously listed here (the LMR36015 EN-to-VIN tie, and the C_FF
20 pF value) are now explained inline in §2.2 above.
