# Power path trace sizing

## Status of the inputs

**There is no power budget in this repository.** The rail structure below was
reconstructed from net names and components referenced in
[`schematic-fixes.md`](schematic-fixes.md); the per-rail currents are *reference
estimates*, not measured or specified values. Every current in the
"Reference" column needs to be replaced with a real number before this goes to
fab — see [What's still needed](#whats-still-needed).

The widths in the master table are exact for whatever current you plug in, so
that part of the work stands regardless.

## Rails in the design

Derived from net labels in the schematic punch list:

| Rail | Evidence | Role |
|---|---|---|
| `+12V_P` | `V12_SENSE` divider taps it (43.2k/10k) | Vehicle 12 V input, post-protection |
| `+5V` | `V5_SENSE` divider taps it (8.2k/10k); aliases `+5V_SH`, `+5V_TC` on `wheel-displays` | Buck output (U1, FB divider R1/R2/C7) |
| `+3V3` | Debug header, MCU | Digital — STM32G474 |
| `+3V3A` | `D_12CL` clamp reference | Analog / ADC reference domain |
| `GND` | — | Return; assumed to be a solid plane |

Known loads: STM32G474 (170 MHz), CAN transceiver (`CAN_RX` on PB8), USB with
USBLC6-2SC6 ESD array, a Sharp-type memory LCD (`LCD_EXTCOMIN` implies the
common-inversion drive of a memory-in-pixel panel), paddle-shift inputs, debug
header.

**The dominant unknown is the shift-light / RGB LED string.** On an FSAE wheel
this is normally the largest single load by a wide margin, and it is not
described anywhere in the repo. The `+5V_SH` alias may well be that rail. If
there is an LED bar, its current sets the `+5V` and `+12V_P` widths and nothing
else comes close.

## Method

IPC-2221 constant-temperature-rise model:

```
I = k · ΔT^0.44 · A^0.725        A = (I / (k · ΔT^0.44))^(1/0.725)
k = 0.048 external layers,  0.024 internal layers
A in mils², ΔT in °C, width = A / thickness   (1 oz = 1.378 mils)
```

IPC-2221 is used rather than the newer IPC-2152 because it is the conservative
choice and needs no assumptions about board construction, adjacent copper, or
airflow. IPC-2152 will typically justify narrower traces on a board with plane
layers — worth revisiting only if routing gets tight.

## Master table — width vs. current

Width required for a given current, by copper weight and layer.

| Current | 1 oz ext, ΔT 10 °C | 1 oz ext, ΔT 20 °C | 2 oz ext, ΔT 10 °C | 2 oz ext, ΔT 20 °C | 1 oz int, ΔT 10 °C |
|---:|---:|---:|---:|---:|---:|
| 0.10 A | 0.5 mil | 0.3 mil | 0.3 mil | 0.2 mil | 1.3 mil |
| 0.25 A | 1.8 mil | 1.2 mil | 0.9 mil | 0.6 mil | 4.6 mil |
| 0.50 A | 4.5 mil | 3.0 mil | 2.3 mil | 1.5 mil | 11.8 mil |
| 0.75 A | 8.0 mil | 5.2 mil | 4.0 mil | 2.6 mil | 20.7 mil |
| 1.00 A | 11.8 mil | 7.8 mil | 5.9 mil | 3.9 mil | 30.8 mil |
| 1.50 A | 20.7 mil | 13.6 mil | 10.3 mil | 6.8 mil | 53.8 mil |
| 2.00 A | 30.8 mil | 20.2 mil | 15.4 mil | 10.1 mil | 80.0 mil |
| 3.00 A | 53.8 mil | 35.3 mil | 26.9 mil | 17.7 mil | 140 mil |
| 5.00 A | 109 mil | 71.5 mil | 54.4 mil | 35.7 mil | 283 mil |

Below roughly 0.5 A the thermal requirement falls under any sane minimum
feature size. **Those traces are sized by voltage drop, impedance to transients,
and mechanical robustness — not by heat.** Do not route a 3.3 V rail at 2 mils
just because the table allows it.

## Per-rail recommendation

Sized against the reference currents, external layer, 1 oz, ΔT ≤ 10 °C, with
practical minimums applied where thermal demand is negligible.

| Rail | Reference current | Thermal minimum | **Recommended width** | Driver of the choice |
|---|---:|---:|---:|---|
| `+12V_P` in | ~1.0 A (est.) | 11.8 mil | **30 mil / 0.75 mm** | Robustness + transient headroom; car-side rail |
| `+5V` | ~2.0 A (est., LED-dominated) | 30.8 mil | **40 mil / 1.0 mm**, pour preferred | Thermal; widen if LED count is high |
| `+3V3` | ~0.25 A (est.) | 1.8 mil | **15 mil / 0.4 mm** | Drop + practical minimum, not heat |
| `+3V3A` | < 20 mA | negligible | **12 mil / 0.3 mm** | Practical minimum; keep away from switching node |
| `GND` | — | — | **Solid plane** | Return path integrity |

Reference currents assume: STM32G474 ~50 mA run, CAN transceiver ~70 mA worst
case dominant, memory LCD low-mA, and a shift-light string in the 0.5–1.5 A
band at 5 V. The 12 V figure back-computes from ~10 W output at ~85 % buck
efficiency. **Replace all of these.**

## Voltage drop check

Thermal sizing does not bound drop. Check it separately:

```
R_trace = 0.5 mΩ × (length_inches × 1000 / width_mils)      [1 oz]
        = 0.25 mΩ × (...)                                    [2 oz]
```

Worked example — `+5V` at 40 mil, 3 in run, 1 oz, 2 A:
squares = 3000/40 = 75 → R = 37.5 mΩ → **75 mV drop** (1.5 %). Acceptable.

Same geometry at 15 mil would be 100 mΩ → 200 mV (4 %), which starts to matter
for LED brightness matching across a bar. Budget ≤ 2 % on `+5V` and ≤ 1 % on
`+3V3` if the ADC accuracy of `V12_SENSE` / `V5_SENSE` matters to you — those
dividers measure the rail, so IR drop between the divider tap and the load is
invisible to the firmware and becomes measurement error.

## Vias in the power path

Budget **~1 A per 0.3 mm (12 mil) plated via** as a conservative rule of thumb,
and use **a minimum of two vias** anywhere a rail above 0.5 A changes layer. For
the `+5V` rail at the reference 2 A that means three or more. Stitch generously
under the buck and around the connector — vias are cheap, a burned-through
barrel on a car is not.

## What's still needed

To turn the reference column into a real budget:

1. **Shift-light / RGB LED count, part number, and drive current at full white.**
   This single number dominates the `+5V` and `+12V_P` widths.
2. Display part number(s) and whether either `+5V_SH` / `+5V_TC` feeds a
   backlight.
3. CAN transceiver part number (dominant-state supply current varies ~2× across
   common parts).
4. Buck (U1) part number and efficiency at the actual load point.
5. Copper weight and layer stack actually being ordered — the table assumes 1 oz
   outer; 2 oz halves every width.
6. Fuse rating on the car side, which should sit above worst-case draw and below
   the `+12V_P` trace's fusing current.

Once 1–5 are known, read the widths straight out of the master table.
