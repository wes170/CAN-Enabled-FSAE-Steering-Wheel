# Instructions — Sim-Rig Use of the Wheel PCB (`FSAE-WHEEL`)

> **Permanent memory file.** As of **Rev B.5 there is no sim variant.** There is one PCB, one
> assembly, one BOM and one firmware image. A car wheel and a sim wheel are the same object; the only
> difference is **which cable is plugged in**. This file covers what that means electrically, what
> the firmware does about it, and how to bring a wheel up on a desk.
>
> Base instructions: `wheel-pcb-altium-instructions.md`. Schematic: `wheel-schematic-complete.md`
> (§3.4 and §3.4a for the USB stage).
>
> *The filename is kept so existing links still resolve. Its old title — "Sim-Rig Variant" — is what
> changed.*

## 1. Why one build, not two (first principles)

The variant scheme existed to save parts: a sim board omitted the CAN transceiver and the whole 12 V
front end, and a car board omitted the one 0 Ω link (`R_VBUS`) that lets USB power the board. Two
fitted-lists, two BOM outputs, two pick-and-place files, and every change reasoned about twice.

What that bought was a few dollars of parts. What it cost is the more interesting number:

- **Two assembly configurations is two chances to build the wrong one**, and the failure is quiet — a
  sim board with the buck fitted works; a car board with `R_VBUS` fitted works; the wrong BOM
  uploaded to the assembler produces a board that works until it does not.
- **Numbers derived per-variant go stale invisibly.** The USB LED cap was 350 mA, correct while the
  CAN transceiver was DNP. It is wrong once the transceiver is fitted, and nothing would have
  prompted anyone to re-check a constant attached to a build flag. See §3 — this actually happened.
- **Every DNP is a question at every gate.** G6 diffs fitted-lists specifically because this is where
  boards go wrong.

Against that, the parts saved are among the cheapest on the board. **One build wins.** The
base-vs-derivative framing goes with it: there is no base and no derivative, just a wheel.

### What is still DNP, and why that is a different thing

| Ref | Why it stays DNP |
|---|---|
| `R_T1`, `R_T2`, `C_T1` | CAN termination is an **installation** decision — fit only if this wheel is a physical end of the bus. Per-car, not per-build |
| `J7`–`J10`, `D9`–`D12` | Satellite encoder **provision**. Fitting `J7` instead of `ENC1` moves a thumb encoder off-board with no respin |
| `R_EXTMODE_L` | The unused half of a strap option on the display's `EXTMODE` pin |

An **option** is a footprint you may or may not use on a given installation. A **variant** forks the
BOM. Only the second one is being deleted.

## 2. Electrical mechanism

Everything is fitted. `R_VBUS` — the 0 Ω link that ORs USB VBUS into `+5V` through the `D6` Schottky,
**downstream of the buck** — is fitted on every board. `D6` points VBUS → board, so board power can
never reach the host's port regardless of what else is connected.

| Cable in | What powers the board | What the firmware becomes |
|---|---|---|
| `J1` Deutsch DTM (vehicle) | 12 V → `F1` → `Q1` → `U1` buck → `+5V` | CAN keypad + broadcast decode, 450 mA LED cap |
| `J2` USB-C (desk / sim rig) | VBUS → `D6` → `R_VBUS` → `+5V` | USB HID gamepad, 300 mA LED cap after enumeration |
| Both | the buck wins at 5.0 V; `D6` reverse-biased at 4.7 V | vehicle personality; USB available for DFU |

**Two things to read before relying on this**, both in `wheel-schematic-complete.md` §3.4a:

1. **USB back-feeds `+12V_P` to ~4 V through the buck's high-side body diode** (defect 8.15). Benign
   with `J1` unmated, which is the desk case. Plugging USB into a wheel that is *also* mated to a
   powered-down car makes the host port try to energise the vehicle bus. Don't.
2. **CAN is not guaranteed on USB power** — the TJA1051T/3 wants 4.75 V minimum and the USB-fed rail
   sits at about 4.7 V. Car cable → CAN, USB cable → HID. That is the design, not a limit discovered
   later.

## 3. USB current budget — and the number the variant scheme hid

With 5.1 kΩ Rd on both CC pins the wheel is a UFP entitled to **500 mA** once enumerated, and 100 mA
before that. The LEDs are not the only load:

| Load on `+5V` | Worst case |
|---|---|
| AP2112K → `+3V3` (MCU + display), LDO so 1:1 | 110 mA |
| TJA1051T/3 transmitting dominant | 70 mA |
| 74AHCT1G125 LED buffer | <1 mA |
| **Non-LED total** | **≈180 mA** |
| **LED headroom** (500 − 180) | **320 mA** |
| **Firmware cap** | **300 mA** |

**The old figure was 350 mA and it is now wrong.** It was right when written — the CAN transceiver
was DNP on sim boards, so 110 + 350 = 460 mA fitted inside 500. Fitting the transceiver everywhere
spends 70 mA that number never saw, and 110 + 70 + 350 = **530 mA** does not fit.

Nothing would have caught that. It was a `#define` selected by a build flag, correct against a BOM
that quietly changed underneath it. **This is the strongest argument for the single build**: the
firmware now *measures* which supply is present instead of being compiled against an assumption
about it.

**No CC-current sensing.** An earlier version of this file promised a DNP `R_CC_SNS` footprint for
detecting a 1.5 A/3 A source and lifting the cap. **That footprint never existed** in the schematic
definition or in either BOM — defect 8.16. It is not being added: 300 mA is safe on any compliant
source, so lifting it is an optimisation, and building it would need an ADC channel number from
DS12288 Table 12 and the USB-C Rp advertisement currents from the Type-C spec, neither of which this
project has verified.

## 4. Firmware — one image, two personalities

There is **no `-DBOARD_SIM`**. `firmware/include/power.h` + `src/power.c` decide at run time.

**Supply detection.** `V12_SENSE` (PA1, ADC12_IN2) through the existing 47 k/10 k divider — no board
change was needed for this. Assert vehicle above **7 V**, release below **6 V**. The thresholds are
set by the back-feed, not by the vehicle: USB parks `+12V_P` near 4 V, so anything at or below about
5.5 V must read as USB. A cranking dip below 6 V briefly selects the USB cap, which is the correct
direction — the LEDs dim while the starter is engaged and the rail is already sagging.

**Unknown means USB.** Every path that cannot prove the vehicle rail returns the tighter cap. Wrong
in that direction dims LEDs; wrong in the other browns out a host port with the display and the CAN
transceiver hanging off the same rail.

**Caps:** vehicle 450 mA · USB enumerated 300 mA · **USB not yet enumerated 0 mA**. The strip stays
dark for the first moments on a USB cable, because the pre-enumeration allowance is 100 mA and the
non-LED load nearly consumes it on its own.

**USB clocking is HSI48 + CRS, not the crystal** (defect 8.17). No PLL configuration serves 170 MHz
SYSCLK and 48 MHz PLLQ at once — the arithmetic is at the end of `clock_config.h`, with a static
assertion that fires if a future re-tune ever changes that. `usb_clock_init()` in `system_init.c`.

**HID descriptor** (unchanged): `Gamepad`, 32 buttons + 8 axes — driver-free on every sim title.
- Buttons 1–6 face buttons · 7–12 encoder push switches · 13/14 paddle up/down.
- Each encoder detent emits a 40 ms virtual button pulse (CW/CCW → buttons 15–26), the standard
  sim-wheel idiom that titles bind "next map"-style functions to.
- Each encoder's absolute accumulated position is also exposed on an axis, for titles that prefer axes.

**Paddles.** On the desk, wire the paddle switches to `J1` pins 4/5 and ground pin 6 exactly as in the
car; the MCU enables internal pull-ups and the same `PADDLE_*_SNS` dividers read them. Nothing about
the paddle path changes between the two cables.

**Display and LEDs** behave identically on both cables, subject to the cap above. Stretch goal
unchanged: accept SimHub-style output HID reports (report ID 2: 24×RGB + 7-seg values) so the game
drives the shift lights.

**DFU** over USB works on every board on either cable — it always did. `R_VBUS` was never in that
path, because DFU needs data, not power.

## 5. Altium

**Delete the `CAR` and `SIM` variants.** One assembly. `[No Variations]` is now the correct thing to
upload rather than the classic mistake. Drop the `.VariantName` special string from the title block
or replace it with the revision — a variant name on a board with no variants is worse than no string.

The OutJob loses its duplicated per-variant BOM and pick-and-place outputs. One of each.

**G6 gate change:** "variant fitted-lists diffed" no longer applies, and is replaced by **"confirm no
fabrication output still selects a variant"** — an OutJob left pointing at a deleted variant is the
one way this change can bite during ordering.

## 6. Bring-up on a desk

1. Plug through a **USB power meter**. Confirm enumeration draw is ≤100 mA **with the LEDs dark** —
   they must stay dark until the host configures the device. If they light before enumeration, the
   cap gating is broken.
2. After enumeration, confirm total draw stays **≤500 mA** with the LED pattern at full demand. The
   cap should hold it near 480 mA worst case.
3. **The back-feed check (defect 8.15), and it is not optional.** With `J1` unmated, measure `+12V_P`
   at `TP1` — expect roughly 4 V, and confirm nothing downstream misbehaves. Watch specifically for
   the buck hiccup-oscillating as its back-fed `VIN` crosses UVLO. Then read `V12_SENSE` in firmware
   and confirm it reports **USB**, not vehicle.
4. Verify HID with `joy.cpl` (Windows) / `jstest` (Linux): all buttons, both paddles, encoder pulses
   in both directions at slow and flick speeds. Missed detents are a firmware quadrature bug, not
   hardware — the TIM encoder peripheral does not miss.
5. **Swap cables while running.** Vehicle → USB: the LED cap must come down on the next frame.
   USB → vehicle: it must come back up. This is the behaviour the single build exists to provide, so
   it gets tested rather than assumed.
6. 30-minute soak at capped-max LED load; the USB meter must stay ≤500 mA.
7. Then run the common bring-up items of `engineering-rigor.md` §4 (the CAN stages need the vehicle
   cable).
