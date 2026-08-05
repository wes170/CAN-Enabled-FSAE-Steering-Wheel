# Power path trace sizing

## Status of the inputs

No power budget existed in this repository; it is derived below from the LED
count and copper weight supplied by the team, plus datasheet figures for the
named parts.

**Confirmed inputs:** 1 oz copper (JLCPCB default). 24 addressable RGB LEDs —
16 in the shift bar, 8 for traction-control activation / lockup.

**Still assumed:** per-LED current of 60 mA at full white (WS2812B class). This
is the one number the whole budget pivots on — see
[LED current sensitivity](#led-current-sensitivity) for how the answer moves if
the part is different.

### The `+5V_SH` / `+5V_TC` aliases are the LED groups

`schematic-fixes.md` filed "`+5V` has multiple names (`+5V_SH`, `+5V_TC`)" under
*no action needed — cosmetic only*. With the 16/8 LED split, those names read as
**SH = shift bar** and **TC = traction control lights**, which makes them the
two LED branch feeds rather than redundant labels.

**Do not rename them to plain `+5V`.** They are the natural place to split the
LED load into separately-sized branches, and the tables below do exactly that.
Collapsing them to one net erases the distinction at layout time.

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

**The LED array dominates the budget**: 1.44 A of the 1.74 A `+5V` total, or
83 %. Everything else on the board is rounding error by comparison, which is why
the sizing work below concentrates on the `+5V` path.

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

## Power budget

Worst case is **all 24 LEDs at full white simultaneously**. That is not a
contrived condition — most teams flash the full array during power-on self test,
and a lockup event can light the TC group while the bar is at redline.

| Load | Current @ 5 V | Basis |
|---|---:|---|
| Shift bar, 16 LED (`+5V_SH`) | 0.96 A | 16 × 60 mA |
| TC / lockup, 8 LED (`+5V_TC`) | 0.48 A | 8 × 60 mA |
| STM32G474 @ 170 MHz | ~50 mA | run mode, peripherals active |
| CAN transceiver | ~70 mA | worst-case dominant |
| LCD + misc | ~50 mA | allowance |
| 3V3 regulator overhead | ~130 mA | included in the 0.30 A logic figure below |
| **`+5V` total** | **1.74 A** | LEDs 1.44 A + logic 0.30 A |
| **`+12V_P` input** | **1.02 A** | 8.7 W out, 85 % efficiency, at Vin = 10 V |

The 12 V figure is computed at **Vin = 10 V, not 12 V** — a buck draws its
highest input current at its lowest input voltage, and a cranking dip is the
worst case for this rail, not nominal running voltage. At 14.4 V charging it is
only ~0.71 A.

## Per-rail recommendation

External layer, 1 oz, ΔT ≤ 10 °C, with practical minimums applied where thermal
demand is negligible.

| Rail | Current | Thermal minimum | **Recommended width** | Driver of the choice |
|---|---:|---:|---:|---|
| `+12V_P` in | 1.02 A | 12.2 mil | **25 mil / 0.64 mm** | Robustness + load-dump headroom |
| `+5V` trunk | 1.74 A | 25.4 mil | **40 mil / 1.0 mm**, pour preferred | Thermal |
| `+5V_SH` branch | 0.96 A | 11.2 mil | **20 mil / 0.5 mm** | Thermal + drop along the bar |
| `+5V_TC` branch | 0.48 A | 4.3 mil | **15 mil / 0.4 mm** | Drop + practical minimum |
| `+3V3` | 0.25 A | 1.7 mil | **15 mil / 0.4 mm** | Drop + practical minimum, not heat |
| `+3V3A` | < 20 mA | negligible | **12 mil / 0.3 mm** | Practical minimum; keep off the switching node |
| `GND` | — | — | **Solid plane** | Return path integrity |

### Copper weight: 1 oz is fine, keep the JLC default

The widest rail needs 25.4 mil of the 40 mil budgeted. No reason to pay for
2 oz.

**One caveat if this is a 4-layer board:** JLCPCB's 4-layer default is 1 oz
outer / **0.5 oz inner**. The `+5V` trunk on a 0.5 oz inner layer would need
~132 mil to hit the same ΔT. Keep the power path on outer layers, or use a
filled plane region rather than a routed trace if it must go inside.

## LED current sensitivity

The budget above assumes 60 mA/LED. If the part differs, the `+5V` trunk moves:

| Per-LED @ full white | String total | `+5V` trunk | Thermal min | Recommended |
|---:|---:|---:|---:|---:|
| 20 mA | 0.48 A | 0.78 A | 8.4 mil | 20 mil |
| 40 mA | 0.96 A | 1.26 A | 16.3 mil | 30 mil |
| **60 mA (assumed)** | **1.44 A** | **1.74 A** | **25.4 mil** | **40 mil** |
| 80 mA | 1.92 A | 2.22 A | 35.5 mil | 50 mil |

The 40 mil recommendation covers everything through 60 mA/LED with margin, and
is only marginally light at 80 mA. **Route 40 mil and the LED part choice stops
being a layout risk.**

A firmware brightness cap does not justify sizing below these numbers unless the
cap cannot be bypassed. Size the copper for what the hardware can draw, not for
what the current firmware asks it to draw.

## Voltage drop check

Thermal sizing does not bound drop. Check it separately:

```
R_trace = 0.5 mΩ × (length_inches × 1000 / width_mils)      [1 oz]
        = 0.25 mΩ × (...)                                    [2 oz]
```

Worked example — `+5V` trunk at 40 mil, 3 in run, 1 oz, 1.74 A:
squares = 3000/40 = 75 → R = 37.5 mΩ → **65 mV drop** (1.3 %). Acceptable.

### Drop along the shift bar

The 16-LED bar is a *distributed* load, not a lump at the end — the trace
current tapers as each LED taps off. For a one-end feed the effective drop is
`I_total × R_total / 2`, not `I × R`.

At 20 mil, 1 oz, across an 8 in bar carrying 0.96 A:

| Feed strategy | Drop at the far end |
|---|---:|
| One end | 96 mV |
| **Centre injection** | **24 mV** |

**Feed the bar from the middle, or from both ends.** It is a free 4× improvement
— same copper, one extra via drop — and it halves the worst-case current in any
one trace segment, which is why 20 mil suffices for a 0.96 A branch.

This matters more than the raw thermal number: WS2812-class parts shift colour
noticeably as VDD sags, and a gradient across a shift bar is visible to the
driver at exactly the moment they are looking at it.

### Rail sense accuracy

Budget ≤ 2 % drop on `+5V` and ≤ 1 % on `+3V3`. The `V12_SENSE` / `V5_SENSE`
dividers measure at their tap point, so any IR drop between the tap and the load
is invisible to firmware and becomes measurement error — tap them as close to
the load as routing allows, not at the regulator output.

## Vias in the power path

Budget **~1 A per 0.3 mm (12 mil) plated via** as a conservative rule of thumb,
and use **a minimum of two vias** anywhere a rail above 0.5 A changes layer. For
the `+5V` trunk at 1.74 A that means **two minimum, three preferred**; the
`+5V_SH` branch at 0.96 A wants two. Stitch generously under the buck and around
the connector — vias are cheap, a burned-through barrel on a car is not.

## Knock-on items the LED count raises

These fall outside trace sizing but are implied by a 1.74 A `+5V` rail, and are
cheaper to fix now than after fab:

1. **Check U1's current rating.** The buck now needs to deliver 1.74 A
   continuous at 5 V. Confirm the part and its inductor are rated for it with
   margin, and check the efficiency curve at that load point rather than at the
   datasheet's headline figure.
2. **The 3V3 regulator may need a package change.** If `+3V3` is an LDO from
   5 V at 250 mA, it dissipates (5 − 3.3) × 0.25 = **0.43 W**. That is too much
   for SOT-23; it wants SOT-223 or a buck. Worth confirming which one is on the
   sheet.
3. **Bulk decoupling for the LED array.** 24 addressable LEDs switching together
   is a substantial transient load. Budget ≥ 100 µF bulk at the `+5V` feed, plus
   100 nF per LED (or per 2–3 LEDs at minimum) local to each package.
4. **Inrush at power-on self test.** If firmware flashes all 24 white at boot,
   that step coincides with bulk-cap charging. Size the car-side fuse for it —
   a 2 A or 3 A fuse suits a 1.02 A worst-case draw without nuisance-blowing.

## Remaining unknowns

Only one input still materially affects trace width:

- **LED part number**, to replace the assumed 60 mA/LED. The
  [sensitivity table](#led-current-sensitivity) shows the 40 mil trunk
  recommendation holds anywhere in the 20–60 mA range, so this is a confirmation
  rather than a blocker.

Lower-impact confirmations: CAN transceiver part number (dominant-state current
varies ~2× across common parts, but it is ~4 % of the budget either way), and
whether the board is 2- or 4-layer, for the 0.5 oz inner-layer caveat above.
