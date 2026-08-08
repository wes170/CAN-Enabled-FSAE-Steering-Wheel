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

### The `+5V_SH` / `+5V_TC` aliases are the LED groups — but need a schematic fix first

`schematic-fixes.md` originally filed "`+5V` has multiple names (`+5V_SH`,
`+5V_TC`)" under *no action needed — cosmetic only*. With the 16/8 LED split,
those names read as **SH = shift bar** and **TC = traction control lights** —
the two LED branch feeds, not redundant labels.

**But as drawn, they really are the same net.** The labels sit on `+5V` with no
component between them, so Altium's compiler merges them: the PCB editor only
ever sees one `+5V` net, not three. That's confirmed in-tool — after compiling,
the PCB panel's Nets list shows no separate `+5V_SH` / `+5V_TC` entries to build
net classes from, which is exactly what merging looks like in practice.

**Fix:** add a 0 Ω link resistor in series on each branch (`R_SH`, `R_TC`, 0805,
between `+5V` and each branch label) — see `schematic-fixes.md` item 7. This is
electrically a no-op but stops the compiler from merging the nets, so
`+5V_SH` / `+5V_TC` come back as real, independently net-classable nets. Do
**not** rename everything to plain `+5V` to clear the warning — that was the
original suggestion and it's the wrong direction; it would make the merge
permanent and remove any way to size or protect the branches independently.

If the link resistors aren't added, fall back to sizing `+5V` as one net for
the full 1.74 A worst case (40 mil trunk width covers it) and taper copper
toward each LED group by hand during layout — DRC just won't enforce the
per-branch split. The [per-rail table](#per-rail-recommendation) below assumes
the link resistors are in place; drop the `+5V_SH` / `+5V_TC` rows and treat
`+5V` as the only rail if you go that route instead.

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

Below roughly 0.5 A the thermal requirement falls under JLC's 4 mil minimum
track width. **Those traces are sized by voltage drop, impedance to transients,
and mechanical robustness — not by heat.** Do not route a 3.3 V rail at 2 mils
just because the table allows it; see
[Checked against JLCPCB capabilities](#checked-against-jlcpcb-capabilities).

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

### Copper weight: 1 oz is fine

The widest rail needs 25.4 mil of the 40 mil budgeted, against a JLC minimum of
4 mil. No reason to pay for 2 oz — and 2 oz would *raise* the minimum feature
size. Details in [Checked against JLCPCB
capabilities](#checked-against-jlcpcb-capabilities).

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

Budget **~1 A per 0.3 mm (12 mil) plated via** as a conservative rule of thumb —
the IPC calculation gives ~1.8 A for that barrel at ΔT 10 °C, so this carries
close to 2× derating. Use **a minimum of two vias** anywhere a rail above 0.5 A
changes layer. For
the `+5V` trunk at 1.74 A that means **two minimum, three preferred**; the
`+5V_SH` branch at 0.96 A wants two. Stitch generously under the buck and around
the connector — vias are cheap, a burned-through barrel on a car is not.

**Use 0.3 mm drill / 0.6 mm diameter vias in the power path**, not JLC's 0.15 mm
minimum. A 0.15 mm barrel carries roughly half the current (~0.6 A derated), and
sub-0.3 mm drills can attract extra tooling cost — smaller is the wrong direction
on both counts here.

## Checked against JLCPCB capabilities

Verified against JLC's published capability page rather than assumed.

| Parameter | JLC capability (1 oz) | Our most demanding use | Margin |
|---|---|---:|---|
| Min track width | 0.10 mm / **4 mil** (2-layer)<br>0.09 mm / **3.5 mil** (multilayer) | 12 mil (`+3V3A`) | **3× clear** |
| Min spacing | same as above | designer's choice | — |
| Min via hole | 0.15 mm | 0.3 mm recommended | 2× above min |
| Min via diameter | 0.25 mm | 0.6 mm recommended | 2.4× above min |
| PTH annular ring | ≥ 0.20 mm | — | — |
| Copper to board edge | ≥ 0.2 mm | — | — |

**Nothing in the recommendation table comes close to a JLC limit.** The
narrowest thing specified is 12 mil against a 4 mil floor. There is no
manufacturability risk in the power path — the widths are set by current and
drop, exactly as they should be.

Note that some resellers and older guides quote 5 mil as the practical minimum
for the cheapest 2-layer tier. Immaterial here for the same reason.

### Where the master table goes below what JLC can build

Rows at 0.5 A and under produce thermal widths of 0.3–4.5 mil, which is at or
under JLC's 4 mil floor. **Those rows are informational, not routable.** Treat
4 mil as a hard floor and 8–10 mil as a sensible practical minimum for anything
carrying real current.

### A second reason to stay at 1 oz

2 oz copper raises JLC's own minimum track/space to **0.16 mm (6.5 mil)** on
2-layer, because thicker copper over-etches laterally. Not binding at our
widths, but it confirms the direction: 1 oz is the right call, and it is the
cheaper tier.

### If this is a 4-layer board

JLC's 4-layer inner-layer default is **0.5 oz**, confirmed. The `+5V` trunk on
0.5 oz inner copper would need roughly **132 mil** for the same 10 °C rise.
Keep the power path on outer layers, or upgrade inner copper to 1 oz at extra
cost. Outer layers on 4-layer support 1 oz / 2 oz; inner supports 0.5 / 1 / 2 oz.

Confirm the copper weight actually selected on the order page at checkout — the
default differs between 2-layer and 4-layer product lines.

## Setting per-net widths in Altium

Yes — via net classes plus a **Width** constraint per class, and the autorouter
honours the Preferred value. **Which dialog you use depends on the project, not
the Altium version** — as of AD 25/26 there are two coexisting rule systems.

### 0. Check which system this project uses

Open the PCB document, click **Design** in the main menu:

- Menu shows **Constraint Manager** → this project uses the new system (default
  for projects created in AD 25+)
- Menu shows **Rules** instead → this project uses the classic PCB Rules and
  Constraints Editor

A one-way migration exists (**Design → Migrate Project to Constraint Manager
Flow**) if you want to move an old project onto the new system, but it isn't
required — the classic editor is still fully supported.

### 1. Create the net classes (same for both systems)

**Prerequisite for `PWR_5V_SH` / `PWR_5V_TC`:** these only work once
`schematic-fixes.md` item 7 (the `R_SH` / `R_TC` link resistors) is placed and
the project recompiled — otherwise `+5V_SH` / `+5V_TC` don't exist as separate
nets to build a class from. `PWR_12V`, `PWR_5V_TRUNK`, and `PWR_3V3` aren't
affected and can be created regardless.

To create each class: open the **PCB** panel, set the dropdown to **Nets**,
select the net(s) belonging to one rail (**Ctrl+click** for more than one),
right-click on the selection → **Add Class**. The **Edit Net Class** dialog
opens — type the class name into the **Name** field, use the **`>`** button to
move any not-yet-included nets from **Non-Members** to **Members**, click
**OK**. Repeat per rail. (Also reachable via **Design → Classes** — the Object
Class Explorer — moving nets between the same two panes.)

### 2a. Constraint Manager path

1. **Design → Constraint Manager**
2. Click **Physical** (top-left) to switch to the physical constraints view
3. Expand the net-class tree (right-click → **Expand All** if a class doesn't
   show) and find your class row
4. Click the cell in the **Width** column for that class — a panel opens below
   with **Min Width / Preferred Width / Max Width**
5. Enter the values from the table below, save with **Ctrl+S**

**No manual priority step here.** Constraint Manager orders priority
automatically — All Nets (lowest) → net class → individual net (highest) — so a
class-level constraint wins over the board default without extra setup.

### 2b. Classic Rules and Constraints Editor path

1. **Design → Rules… → Routing → Width** → right-click → **New Rule**
2. Set the rule's scope query to `InNetClass('PWR_5V_TRUNK')` (or
   `InNet('+5V')` for a single net)
3. Enter **Min Width / Preferred Width / Max Width**
4. Repeat per class

| Net class | Nets | Min | **Preferred** | Max |
|---|---|---:|---:|---:|
| `PWR_12V` | `+12V_P` | 15 mil | **25 mil** | 60 mil |
| `PWR_5V_TRUNK` | `+5V` | 26 mil | **40 mil** | 120 mil |
| `PWR_5V_SH` | `+5V_SH` | 12 mil | **20 mil** | 60 mil |
| `PWR_5V_TC` | `+5V_TC` | 8 mil | **15 mil** | 60 mil |
| `PWR_3V3` | `+3V3`, `+3V3A` | 10 mil | **15 mil** | 40 mil |
| *(existing catch-all)* | `All` | 5 mil | 8 mil | 20 mil |

How the three values are used, in both systems:

- **Preferred** — what the autorouter and interactive routing actually lay down.
  Press **3** while placing a track in interactive routing to cycle
  min/preferred/max/custom live.
- **Min / Max** — enforced by online and batch DRC, and bound what interactive
  routing will let you draw.

Set **Min at or just above the thermal minimum**, so DRC catches a trace necked
down to squeeze past a via. Set **Max generously** — if Max equals Preferred you
cannot hand-widen a trace or fatten a polygon connection without tripping DRC.

**This is the step people miss in the classic editor, and the reason the
Constraint Manager table above says "no manual step":** the classic dialog
applies only the single highest-priority *matching* rule, and the existing
catch-all `All` Width rule matches your nets too. If it sits above the new
rules, they silently do nothing. Open **Priorities…** at the bottom of the
Rules dialog, select each new rule, **Increase Priority** to move it above the
catch-all. Verify with **Tools → Design Rule Check**, or right-click a net →
**Applicable Unary Rules** to confirm which rule actually binds. Constraint
Manager projects skip this entirely — that's the practical advantage of the
newer system for exactly this kind of per-net-class setup.

### Situs report errors seen in practice

First autoroute attempt on this board produced 39 errors and 4 warnings.
Recorded here since both classes of problem are likely to resurface on a
re-route.

**39× `Pad ... Appears to be unroutable. Violation against Rule - Clearance
Clearance Constraint (Gap=0.254mm) (All),(All)`** — this is the board-wide
**Clearance** rule (a different rule type from Width, untouched by the setup
above), still at Altium's 10 mil default. Hit fine-pitch/dense parts: the
MCU (LQFP corner pins), a USB-C receptacle (`A4/A5/A9/B4/B5/B9` pin naming),
LED pads, and clustered decoupling caps — pads on these sit closer than 10 mil
apart even before routing starts, so Situs correctly refuses. JLC's real floor
is 0.10 mm (3.9 mil) 2-layer / 0.09 mm multilayer (see
[Checked against JLCPCB capabilities](#checked-against-jlcpcb-capabilities)),
so 10 mil was never load-bearing. **Fix: lower the board-wide `All`–`All`
Clearance rule to 0.15 mm (6 mil)** — ~1.5× the fab floor, clears dense parts
without going anywhere near unmanufacturable. If specific fine-pitch pads are
still tight at 6 mil, add a second, higher-priority Clearance rule scoped to
just that component (`InComponent('U3')`) rather than shrinking the whole
board further.

**4× `Preferred routing width is greater than some pad dimensions... Consider
adding a new SMD Neckdown Rule`** — informational only. Fires whenever
Preferred width exceeds some component's own pad width (e.g. a 0402 cap);
Situs auto-necks the track at the pad regardless. Safe to ignore.

*Optional, if you want explicit control over the taper rather than leaving it
to Situs:* the rule lives under a different category than Width/Clearance —
**SMT → SMD Neck-Down** in the classic editor, or the SMT section of
Constraint Manager's Physical view. Single field, **Neck-Down %** — the max
ratio of track width to pad width before Situs is forced to taper further.
No enforced default; 50% is a reasonable starting point (against a 40 mil
trunk trace and ~20–24 mil 0402 pads, that lands the neck around 10–12 mil,
matching the practical-minimum widths already in this doc). Scope it `All` —
one board-wide rule, not per-net. This changes how the taper is drawn, not
whether the connection is valid; skipping it entirely is fine.

**Catch hiding in the warning text, not the error count:** the `+5V` rule as
actually entered read `Min=0.254mm (10mil) / Max=3.048mm (120mil) /
Preferred=0.381mm (15mil)` — Max matches the trunk spec above, but Min/Preferred
had been left at the `+3V3` pattern (10/15/40) instead of the trunk's 26/40/120.
15 mil at the trunk's 1.74 A worst case runs **~24 °C rise**, more than double
the 10 °C design target (40 mil is ~5 °C — see the
[verification](#method) for the formula). Worth an explicit double-check against
this table after entering rules by hand: the report will state each rule's
actual Min/Max/Preferred verbatim, which makes catching an out-of-family value
easy — just diff it against the table above.

### 3. Don't autoroute the power path

The rules will make Situs use the right widths, but width is not the hard part
of power distribution. An autorouter will not know to:

- feed the shift bar from the **centre** rather than one end (the 4× drop
  improvement above — it is a topology decision, and the router optimises length)
- keep the buck's switching node small
- keep the return path under the outgoing trace

**Route `+12V_P`, `+5V`, `+5V_SH`, `+5V_TC` and the buck loop by hand first, lock
them** (select → Properties panel → Lock), then let the autorouter or ActiveRoute
handle the signal nets around them. The rules still earn their keep: they drive
DRC and constrain your interactive routing, which is where the real protection is.

For the `+5V` trunk specifically, a **polygon pour** on the net is often better
than a 40 mil track — lower resistance and it doubles as a heat spreader for U1.

If you do autoroute, there is a separate **Routing → Routing Priority** rule that
sets which nets get routed first; give the power nets high values so they claim
the good paths.

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
