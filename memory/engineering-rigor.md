# Engineering Rigor — Standing Rules, Gates, and Lessons Learned

> **Permanent memory file.** This file is the project's conscience. It gets *appended to* every time
> something bites us; rules are never deleted, only superseded with a note. Anything here overrides
> convenience.

## 1. Standing rules

1. **First principles or it doesn't ship.** Every part, value, and layout choice must have a stated
   physical reason (see `hardware-selections.md` style). "The reference design did it" is a starting
   point for analysis, not a justification.
2. **Assumptions are tracked, numbered, and closed.** Open assumptions live in
   `system-architecture-and-can.md` §5 (A1–A5 currently). No board order while an assumption that
   could force a respin is open — protocol assumptions (A1, A2) can be closed with a $30 USB-CAN
   sniffer *before* spending board money.
3. **Worst case, not typical.** Budgets (current, voltage drop, temperature, timing) use datasheet
   worst case; where the datasheet is vague (WS2812 current — A3), we measure and write it down.
4. **Everything that leaves the board gets conditioned.** Series impedance → filtering → clamp, at
   the connector, on every line, both boards. No exceptions for "it's just a button."
5. **Derating:** fuses at ≥1.5× continuous load; TVS standoff ≥ rail, clamp ≤ downstream abs-max
   (verified with a scope, not just datasheets); electrolytics/X7R voltage ≥2× rail; connector pins ≤50% rated current.
6. **Protocol claims require two independent sources or one bench measurement.** (Applied: broadcast
   protocol = official spec; keypad = Blink manual + HPA forum + MaxxECU docs; IO12 = PT Motorsport
   emulator source — Box B remains open as A2.)
7. **Single source of truth for interfaces.** Connector pinouts and MCU pin maps live in the Altium
   instruction files' §0 tables. Firmware, harness drawings, and schematics copy *from* there.
8. **Version control everything** — Altium project, OutJobs, this memory folder. A design that isn't
   in git didn't happen. Tag the commit that each board order was generated from (`wheel-revA-ordered`).
9. **Change one thing at a time during bring-up and debugging.** Record every observation in a
   bring-up log file per board (`hardware/<board>/bringup-log.md`).
10. **The car is a hostile environment**: assume 12 V transients, ESD through the driver, vibration,
    120 °C near-tunnel temps for the dash loom, sweat/rain on the wheel. Any part not rated for its
    micro-environment needs a written waiver + test.

## 2. Review gates (must pass in order; sign & date in the project log)

| Gate | Content |
|---|---|
| **G1 — Schematic review** | ERC zero errors; every suppression justified in a comment; pin-map tables in docs match schematic (someone *else* reads them aloud); power tree drawn and budget re-added; every connector pin accounted for |
| **G2 — Layout review** | DRC zero errors; 3D collision vs. mechanical STEP; L2 GND unbroken (visual sweep); protection-at-connector placement audit; hot-loop audit on switchers |
| **G3 — Paper build** | 1:1 print taped to the real wheel/panel; hands on it; encoder/button reach test with gloves |
| **G4 — Netlist cross-check** | Independent re-derivation of J1/J2 pinouts from the schematic vs. §0 tables vs. harness drawing |
| **G5 — Peer sign-off** | A second person reviews G1–G4 evidence; their name goes in the log |
| **G6 — Pre-order** | BOM availability re-checked same-day; variants' fitted lists diffed; gerbers visually inspected in a third-party viewer (not Altium) |

## 3. Design-for-safety specifics of this project (do not lose these)

- **Never power the wheel from an ECU 5 V sensor rail** — it's sized for sensors; LED load will brown
  it out and take sensors with it. The dash's fused 5 V AUX is the wheel's only approved source.
  (Origin: first-principles catch during architecture, 2026-07.)
- **Paddle path must work with wheel electronics dead.** It's pure copper + TVS; any future "smart
  paddle" idea must keep a passive fallback. Shifting is a driver-safety function.
- **Dash shows stale-data indicators.** A frozen coolant temp reads as "fine" while the engine cooks.
  500 ms CAN silence → dashes/greyed values. This is a firmware *requirement*, not a nicety.
- **LED global current cap lives in one function** with a compile-time ceiling per variant
  (CAR 450 mA, SIM 350 mA). Any pattern code goes through it; no direct strip writes.
- **CAN termination is an installation decision** (DNP footprints both boards) — document per-car in
  the harness drawing which node is terminated. Two terminations at one end = reflections = 
  intermittent bus errors that look like firmware bugs.

## 4. Staged bring-up procedure (per board; log everything)

1. **Visual + meter:** solder inspection under magnification; continuity: all GNDs; resistance rail-to-GND (expect >100 Ω) *before* first power.
2. **Power-only:** bench supply, current-limited (wheel: 5 V @ 100 mA limit first; dash: 12 V @ 200 mA). Check rails ±3%, thermal camera / finger sweep.
3. **SWD:** Tag-Connect attach, read MCU ID, flash blinky, verify 3.3 V under load.
4. **CAN loopback:** FDCAN external-loopback + sniffer; then two-node bench bus with USB-CAN at 1 Mbit/s; verify bit timing with scope (sample point ~80%).
5. **Protocol close-out:** emulated keypad against NSP/bench ECU (closes A1); IO12 frames visible in NSP as AVI values (A2 for Box B); broadcast decode against known ECU values.
6. **HMI:** every encoder detent count CW/CCW ×20 fast/slow, every switch 100 presses, sense lines' voltages in both paddle states.
7. **LEDs/display:** current at 100% white measured (closes A3); sunlight test outdoors (wheel LCD, dash panel behind its lens — closes L7 risk).
8. **Environment:** 1 h thermal soak at 60 °C running; vibration shake (attach to a shaker or, minimum, the actual car at idle + rev sweeps) while logging CAN for dropouts.
9. **In-car:** full harness, ECU mapping session with tuner, driver gloves-on usability pass.

## 5. Lessons learned (append-only)

- **L1 (2026-07, research):** Haltech's "Rotary Trim Module" is just a resistor ladder into an AVI —
  meaning *any* absolute-position input is representable as an AVI voltage via IO12 emulation. Model
  the ECU's view first; it simplifies the whole input architecture.
- **L2 (2026-07, research):** Haltech publishes the broadcast (read) protocol but *not* the device
  write protocols; they hand write-protocols to device owners on request, and PT Motorsport's
  open-source emulator fills the IO12 gap. Ask vendors for docs before reverse-engineering.
- **L3 (industry-standard, adopted):** JLC PCBA part rotations routinely mismatch Altium footprints
  — always correct in the order preview and back-annotate a `JLC-Rotation` parameter.
- **L4 (2026-07, design):** WS2812 data needs ≥0.7·VDD input high (3.5 V at 5 V rail) — 3.3 V logic
  violates it marginally and "works on the bench, fails hot." Budget the 74AHCT1G125 from day one.
- **L5 (2026-07, design):** Reflective memory LCD beats any emissive display in direct sun *by
  physics*; picking display tech = picking the lighting environment first.
- **L6 (vendor-risk, adopted):** Display module pinouts move between revisions (Riverdi EVE gens,
  FPC contact flips top/bottom). Transcribe the pin table of the *purchased revision* into the
  schematic and re-verify at incoming inspection.
- **L7 (2026-07, design):** A 1000-nit panel behind the wrong lens is a 300-nit dash. The lens
  (AR coating, haze) is part of the optical budget — test the stack, not the panel.
- **L8 (2026-07, architecture):** Stating the interface as a connector pinout table *first*
  ("5 lines only") caught the wheel-power-source problem before any schematic existed. Keep writing
  interface contracts before drawing.

## 6. Memory-file maintenance

- These six files are the project's permanent memory. Update them **in the same commit** as the
  change they describe. A doc that lags the design is worse than no doc.
- Each Altium/firmware artifact references back to the doc section it implements; when they disagree,
  stop and reconcile before proceeding.
