# Engineering Rigor — Standing Rules, Gates, and Lessons Learned

> **Permanent memory file.** This file is the project's conscience. It gets *appended to* every time
> something bites us; rules are never deleted, only superseded with a note. Anything here overrides
> convenience.

## 1. Standing rules

1. **First principles or it doesn't ship.** Every part, value, and layout choice must have a stated
   physical reason (see `hardware-selections.md` style). "The reference design did it" is a starting
   point for analysis, not a justification.
1a. **No number enters a document until it has been read out of a datasheet.** Pin numbers, pin
   names, package pin counts, component values, current and voltage limits — all of it. Detail
   written from memory *looks* identical to detail that was verified, which is exactly what makes it
   dangerous: it propagates into firmware, harness drawings, and the board order at the same time.
   Where a value is genuinely a placeholder, the text must say "spec — select at capture" rather
   than presenting it as a decision. Status is tracked per part in `datasheet-verification.md`.
   (Origin: lesson L16 — four defects, three of them in one table.)
2. **Assumptions are tracked, numbered, and closed.** Open assumptions live in
   `system-architecture-and-can.md` §5 (A1–A8 currently). No board order while an assumption that
   could force a respin is open — protocol assumptions (A1, A2) can be closed with a $30 USB-CAN
   sniffer *before* spending board money.
3. **Worst case, not typical.** Budgets (current, voltage drop, temperature, timing) use datasheet
   worst case; where the datasheet is vague (WS2812 current — A3), we measure and write it down.
3a. **Check lifecycle status, not just stock.** Every part needs an explicit *Active / NRND / EOL*
   check at selection and again at G6 — a part can be in stock and obsolete at the same time, and
   distributor stock says nothing about whether you can buy it again next year. Record the status in
   the availability table, not just a quantity. (Origin: lesson L14.) Also read *lead time* correctly:
   a long "manufacturer lead time" beside in-stock quantity is the restock time, not your ship date.
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
   in git didn't happen. Tag the commit that each board order was generated from (`wheel-revB-ordered`).
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
| **G4 — Netlist cross-check** | Independent re-derivation of J1/J2 pinouts from the schematic vs. §0 tables vs. harness drawing — **including which ground each signal references** (see L13) |
| **G5 — Peer sign-off** | A second person reviews G1–G4 evidence; their name goes in the log |
| **G6 — Pre-order** | BOM availability re-checked same-day; variants' fitted lists diffed; gerbers visually inspected in a third-party viewer (not Altium) |

## 3. Design-for-safety specifics of this project (do not lose these)

- **Never power either board from an ECU 5 V sensor rail** — it's sized for sensors and shared with
  them; the wheel's LED load will brown it out and take the sensors with it. (Origin: first-principles
  catch during architecture, 2026-07.) **Rev B:** the wheel now runs from vehicle 12 V and regulates
  locally, so it is standalone — it no longer depends on the dash. The dash's 5 V AUX output is deleted.
- **ARB servo safety is firmware-enforced and non-negotiable** (full spec: `system-architecture-and-can.md`
  §3A.3). Four rules: hold last position on CAN loss (**never** spring to centre — an ARB step change
  mid-corner is a handling event); firmware endstops on pulse width (a servo against a hard stop draws
  stall current until it burns); slew limiting; commanded-vs-measured divergence alarm. Any change to
  the servo task requires re-testing all four.
- **Servo power never crosses the dash PCB or its connectors.** ~10 A of stall current versus a 7.5 A
  DTM pin rating, next to a display and an 8-channel analog front end. Signal + ground reference only;
  the BEC's ground must star back to dash GND or the PWM reference floats.
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
2. **Power-only:** bench supply, current-limited (**both boards now 12 V**; wheel @ 150 mA limit first, dash @ 200 mA). Check every rail ±3% — on the wheel that is `+12V_P`, `+5V`, `+3V3`; thermal camera / finger sweep, paying attention to the buck inductor.
3. **SWD:** Tag-Connect attach, read MCU ID, flash blinky, verify 3.3 V under load.
4. **CAN loopback:** FDCAN external-loopback + sniffer; then two-node bench bus with USB-CAN at 1 Mbit/s; verify bit timing with scope (sample point ~80%).
5. **Protocol close-out:** emulated keypad against NSP/bench ECU (closes A1); IO12 frames visible in NSP as AVI values (A2 for Box B); broadcast decode against known ECU values.
6. **HMI:** every encoder detent count CW/CCW ×20 fast/slow, every switch 100 presses, sense lines' voltages in both paddle states.
7. **LEDs/display:** current at 100% white measured (closes A3); sunlight test outdoors (wheel LCD, dash panel behind its lens — closes L7 risk).
8. **Servos (dash only):** the five-step bench procedure in `dash-pcb-altium-instructions.md` §3 — endstops proven *before* the servo touches a linkage.
9. **Environment:** 1 h thermal soak at 60 °C running; vibration shake (attach to a shaker or, minimum, the actual car at idle + rev sweeps) while logging CAN for dropouts.
10. **In-car:** full harness, ECU mapping session with tuner, driver gloves-on usability pass.

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
- **L9 (2026-07, Rev A→B):** *A requirement stated as a voltage was really a requirement about wire
  count.* "The wheel only needs 5 V, ground, paddles, and CAN" was honoured literally in Rev A, which
  forced the dash to become the wheel's power supply and coupled two otherwise independent products.
  Changing the pin to 12 V kept the actual requirement (five functions, one small connector) and
  deleted the coupling. **Ask what a requirement is protecting before designing to its literal text** —
  and when the existing harness already carries what you need, that is evidence about the real constraint.
- **L10 (2026-07, Rev B):** The right question for a rail is not "what voltage do the parts want" but
  "what does the *connection* have to survive." The wheel's power crosses slip rings and a quick-release
  — wearing contacts. At 5 V, 500 mΩ of contact degradation eats half the regulator headroom; at 12 V
  it is unmeasurable. **Pick the distribution voltage from the connector's ageing behaviour, then
  regulate locally.** A useful side effect: 12 V made reverse-polarity protection affordable (P-FET at
  ~20 mV) where at 5 V a diode drop was rejected as too costly, so robustness improved twice over.
- **L11 (2026-07, ARB):** Before adding an output, compute its worst-case power *at the connector*.
  Two ARB servos looked like "two more pins" and are actually ~74 W / ~10 A against a 7.5 A pin rating.
  The design collapsed to something simple (buffered signal only, power external) the moment the
  arithmetic was done. **Do the wattage before drawing the connector.**
- **L12 (2026-07, ARB):** New features are cheapest when they ride existing traffic. ARB control needed
  no new CAN IDs because the wheel already broadcasts encoder positions as IO12 AVI frames and CAN is
  multi-master — the dash just listens. Driver adjustment, wheel-display readout, and ECU logging all
  came free. **Look for what the bus already carries before defining a new message.**
- **L13 (2026-07, ARB — caught in self-review, kept as a warning):** The first J2 allocation put the two
  servo PWM signals on the DAQ connector, leaving one ground pin shared between eight analog returns and
  two digital signal returns. Current in a shared return becomes **offset on every DAQ channel**, and a
  PWM signal must reference the ground its *receiver* uses (the servo BEC stars to power ground, not
  sensor ground). Moving the signals to J1 — onto the pins the 12 V change had just freed — fixed both.
  **Ground allocation is a signal-integrity decision, not a pin-count exercise: count returns, not just
  conductors, and ask what each ground is referencing.** Note also that this error survived the first
  write-up and was only caught on a re-read — which is exactly what gate G4 exists for.

- **L14 (2026-07, BOM validation — the most expensive near-miss so far):** The Rev A thumb encoder
  (Panasonic EVQ-WK4001) was **obsolete** when selected. It passed the original check because it was
  in stock at four distributors — stock is not lifecycle status. Worse, the follow-up revealed the
  *entire edge-drive thumbwheel category* is discontinued (EVQ-WK EOL, EVQWGD001 roller encoder EOL,
  Grayhill 62T >$100 and oversized), so a like-for-like swap did not exist. Three rules came out of it:
  **(a)** check Active/NRND/EOL explicitly, always (now standing rule 3a);
  **(b)** when a part is EOL, check whether its *category* is dying before searching for a drop-in —
  if the category is gone, the architecture has to change, not the part number;
  **(c)** prefer parts whose function can be relocated to a satellite board (§4.1b of
  `hardware-selections.md`), so the next EOL costs a $5 respin instead of a main-board spin.
- **L15 (2026-07, BOM validation):** Distributor pages show *manufacturer lead time* next to
  *in-stock quantity*, and the two are unrelated. A 25-week lead time beside 871 units in stock means
  "ships today, and 25 weeks if we run out" — not "you wait 25 weeks." Nearly rejected a perfectly
  good in-stock part (Bourns PEC09) over this. Read the stock number first; also check whether the
  specific variant is a stocked reel/bulk part or a non-stocked tray part (`T00xx`), since variants of
  the same series differ.

- **L16 (2026-07, datasheet audit — the worst process failure in the project so far):** The schematic
  instructions were written from memory, not from datasheets, and reviewed as if they had been
  verified. A single direct question ("did you read every datasheet?") exposed four defects in under
  an hour, three of them in the *same* pin-map table that the docs instructed the team to **freeze as
  the single source of truth**:
  **(a)** four of six encoder pairs could not do hardware quadrature, because encoder mode needs
  CH1+CH2 of one timer and the pins had been chosen for looking adjacent;
  **(b)** the LED data pin collided with an encoder's timer channel;
  **(c)** CAN_RX landed on `PB8-BOOT0`, and since an idle CAN bus is recessive-high, the board would
  have booted into the system bootloader every time it was powered with a live bus — working
  perfectly on the bench with the bus unplugged;
  **(d)** the Sharp LCD's `EXTMODE` strap pin was omitted entirely, leaving COM inversion undefined
  and risking permanent panel damage.
  None of these were exotic. All four were one table-lookup away. **Plausible-sounding detail is the
  most dangerous kind of wrong, because it survives review** — a reviewer checks whether the pin map
  is self-consistent, not whether it was invented. Hence standing rule 1a, and hence
  `datasheet-verification.md`, which requires a written finding per part before that part is trusted.
- **L17 (2026-07, package-driven conflict):** Peripheral availability is a *package* property, not a
  chip property. FDCAN1 exists on three pin pairs in the STM32G474 die but only two are bonded on
  LQFP-64, and one of those is the USB pair — so choosing USB forced CAN onto the BOOT0 pin. There
  was no way to have USB, CAN, and a clean BOOT0 on this package simultaneously; the conflict had to
  be *managed* (option bits) rather than routed around. **Check peripheral pin availability in the
  specific package before committing to a package, not after.**

- **L18 (2026-07, verification pass 3 — two more, and one of them was free money):**
  **(a)** The DAQ front end clamped 12-bit ADC inputs with a **BAT54S**. Schottky reverse leakage
  (2 µA at 25 °C, ~100 µA at 100 °C) flows into the 5 kΩ Thévenin source and *is* signal: 10 mV of
  offset cold, **500 mV hot** — 20 % error, in a dash we specifically chose for direct sun. BAV199
  (3 pA) fixes it for the same money. **Match the diode class to the node impedance: on a
  high-impedance analog node, leakage is a signal-path parameter, not a leakage-path footnote.**
  **(b)** The Riverdi display's backlight is on a **separate BLVDD rail**, not the module's 3.3 V —
  and it draws **353 mA at 5 V**, not the 1.2 A the docs claimed. That single wrong sentence had
  sized the 3.3 V buck for a load that did not exist and pushed the dash toward a bigger, different
  60 V converter than the wheel. Reading the datasheet *reduced* the BOM: one converter part now
  covers both boards. **Verification is not only a hunt for defects — unverified numbers are
  padded numbers, and padding costs parts.**
- **L19 (2026-07):** Six defects so far, and the two most expensive (BOOT0-on-CAN, TVS-above-abs-max)
  were both **interactions between two correct-looking choices**, not errors in either one alone.
  PB8 is a fine CAN pin. SMBJ33A is a fine TVS. A 35 V buck is a fine buck. Each fails only in
  combination. **Review pairs, not parts:** for every component, ask what else touches its net and
  what the other end of that net does at power-on, at fault, and when hot.

## 5A. Planned future work (do not lose track of these)

| Item | Status | Where specified |
|---|---|---|
| **Servo power conditioning board** (4th board in the family) | Planned — servo power is a harness BEC for now | `system-architecture-and-can.md` §3A.4 |
| IO12 Box B emulation for DAQ re-broadcast | Blocked on assumption A2 | `system-architecture-and-can.md` §2.2, §5 |
| SimHub output-report support (game-driven shift lights) | Stretch | `sim-variant-instructions.md` §4 |
| PLCC-3535 + constant-current LED fallback | Contingency if WS2812 sunlight performance disappoints | `hardware-selections.md` §5 |

**Rule for the servo BEC in the interim:** "temporary" must not mean "unreviewed." Until the
conditioning board exists, BEC selection, its fusing, and its grounding are harness design
deliverables and pass the same gates (G1–G6) as a PCB. The single biggest reason to build the board
is transient isolation — servos are the dirtiest load on the car, and today they share a battery rail
with a display and an analog front end.

## 6. Memory-file maintenance

- These six files are the project's permanent memory. Update them **in the same commit** as the
  change they describe. A doc that lags the design is worse than no doc.
- Each Altium/firmware artifact references back to the doc section it implements; when they disagree,
  stop and reconcile before proceeding.
