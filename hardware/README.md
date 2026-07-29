# `hardware/` — working tree for the three PCBs

This directory is what the memory files tell you to create. It holds the Altium projects, the shared
library, the orderable BOMs, and the per-board bring-up logs.

```
hardware/
├── lib/                     FSAE-Common.SchLib / .PcbLib  (shared by all boards)
├── wheel/                   FSAE-WHEEL.PrjPcb + outputs
│   ├── bom-FSAE-WHEEL-revB.csv
│   └── bringup-log.md
├── dash/                    FSAE-DASH.PrjPcb + outputs
│   ├── bom-FSAE-DASH-revB.csv
│   └── bringup-log.md
└── harness/                 wiring drawings, connector pinouts, servo BEC branch
```

## Order of work

1. **Buy the long-lead parts now** — before any PCB is ordered. See the BOM `Lead` column.
   The Riverdi dash display is single-source with thin stock; it is the part most likely to stall
   the build, and nothing about the PCB design depends on having ordered the boards first.
2. Build the shared library (`lib/`) per `memory/wheel-pcb-altium-instructions.md` §2.
3. Wheel schematic → ERC → layout → DRC, following the wheel instructions.
4. Dash, following `memory/dash-pcb-altium-instructions.md` (deltas only — the generic steps live in
   the wheel doc).
5. Gates **G1–G6** in `memory/engineering-rigor.md` §2, signed off in `/PROJECT-LOG.md`.
6. Order boards. Bring up per `memory/engineering-rigor.md` §4, logging in `bringup-log.md`.

## BOM caveat (read before ordering)

The BOMs list **verified part numbers** where a specific device was selected and checked, and
**specifications** (value, package, rating) where the exact part is a commodity to be picked at
schematic capture. Anything in the `Source / PN` column reading `spec — select at capture` is
deliberately not a part number yet; do not paste those into a distributor cart.

Passive values around the switching regulators come from the datasheet reference designs and must be
confirmed against the **exact** regulator datasheet during capture — see rigor rule 1. Capacitor
voltage ratings on the 12 V input side are ≥50 V on purpose (derating rule 5, plus load-dump margin).
