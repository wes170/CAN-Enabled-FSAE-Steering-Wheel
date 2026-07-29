# Instructions — Sim-Rig Variant of the Wheel PCB (`FSAE-WHEEL`, variant `SIM`)

> **Permanent memory file.** The sim wheel is **the same PCB** as the car wheel — one layout, two
> assembly variants. This file covers the Altium variant mechanics, the electrical differences, and
> the USB HID firmware notes. Base instructions: `wheel-pcb-altium-instructions.md`.

## 1. Why a variant, not a second board (first principles)

A second layout doubles NRE, doubles respin risk, and forks the BOM; the sim use case differs only
in *power source* (USB VBUS instead of dash 5V), *host interface* (USB HID instead of CAN), and
*paddle consumer* (MCU instead of Nexus). All three differences are populate/depopulate + firmware —
exactly what Altium assembly variants exist for. The car remains the base design; sim is the derivative.

## 2. Electrical mechanism (already designed into the base schematic)

| Difference | Base-schematic provision | CAR variant | SIM variant |
|---|---|---|---|
| Power from USB VBUS | `R_VBUS` 0 Ω link after BAT60A Schottky OR-ing VBUS into `+5V` | **DNP** (car power never backfeeds a PC; port is data/DFU only) | **Fitted** — board runs from USB |
| CAN unused | TJA1051 + PESD2CAN + termination | Fitted | **DNP** (saves cost; FDCAN pins idle) |
| Paddles read by MCU | 100 k sense taps `PADDLE_*_SNS` always routed | MCU passively observes ECU-pulled lines | MCU enables internal pull-ups; paddle switches short to GND through the same J1 pins — jumper plug on J1 not required because pull-ups + switch-to-GND are self-contained… **wire the paddle switches to J1 pins 4/5 and GND pin 6 exactly as in the car** |
| Paddle TVS | SMAJ24CA | Fitted | Fitted (harmless) |
| Wheel-side 5V input protection | polyfuse + SMBJ5.0A | Fitted | Fitted (protects PC port too) |

**USB current budget (first principles):** with plain 5.1 k CC pull-downs a UFP may draw 500 mA
(default USB power) — the full-white LED worst case (0.86 A) exceeds it. Firmware in SIM mode
**must cap aggregate LED current to ≤350 mA** (≈40% global brightness — indoors this is plenty;
sunlight math doesn't apply to a sim rig). Optionally read CC line voltage (ADC on a divider — DNP
footprint `R_CC_SNS` provided) to detect a 1.5 A/3 A source and lift the cap.

## 3. Altium variant setup (exact steps)

1. **Project ▸ Variants…** opens Variant Management.
2. Add variant `SIM` (base design = car build; also add explicit variant `CAR` if you want both labeled).
3. For `SIM`, set **Not Fitted**: `U_CAN` (TJA1051), `D_CANTVS` (PESD2CAN), `R_T1`, `R_T2`, `C_T1` (already DNP in base), and set **Fitted**: `R_VBUS`.
4. For `CAR`, set **Not Fitted**: `R_VBUS`.
5. Title block: place a special string `.VariantName` on silk/assembly drawing so built boards are identifiable.
6. Outputs: in the OutJob, duplicate the BOM + pick-and-place outputs and set each one's **Variant**
   (`[No Variations]` is *not* what you upload — always pick `CAR` or `SIM`). Order JLC assembly per variant.
7. Sanity gate: open **3D view per variant** (View ▸ Variants toolbar selector) and confirm the CAN
   transceiver disappears in SIM.

## 4. Firmware notes (SIM build flag)

- **USB stack:** STM32Cube USB Device, class HID. Descriptor: `Gamepad` — 32 buttons + 8 axes is a
  safe, driver-free profile every sim title accepts.
  - Buttons 1–6: face buttons. 7–12: encoder push switches. 13/14: paddle up/down.
  - Each encoder detent emits a 40 ms virtual button pulse (CW/CCW pairs → buttons 15–26) — this is
    the standard sim-wheel encoder idiom (games bind "next map"-style functions to it).
  - Additionally expose each encoder's absolute accumulated position on an axis (some titles prefer axes).
- **LED/display:** default SIM behavior = brightness-capped idle animation + encoder value display
  (display works identically to car mode). Stretch: accept SimHub-style output HID reports
  (report ID 2: 24×RGB + 7-seg values) so the game drives shift lights; protocol doc in firmware repo when built.
- **Mode detection:** firmware detects VBUS-present + no-CAN-traffic → SIM personality at boot; or
  hard-set by the build flag. Never let the car build enumerate USB while CAN is live (draw contention
  on `+5V` through the OR diode is blocked by DNP `R_VBUS` in CAR anyway).
- **DFU:** both variants keep BOOT0 test point + USB DFU as the no-tools reflash path.

## 5. Bring-up deltas

1. First plug through a **USB power meter** — confirm ≤100 mA enumeration draw before LEDs enable.
2. Verify HID with `joy.cpl` (Windows) / `jstest` (Linux): all buttons, both paddles, encoder pulses both directions at slow and flick speeds (missed detents = firmware quadrature bug, not hardware — the TIM encoder peripheral doesn't miss).
3. 30-minute soak at capped-max LED load; USB meter must stay ≤500 mA.
4. Then run the common bring-up items of `engineering-rigor.md` §4 that apply (skip CAN stages).
