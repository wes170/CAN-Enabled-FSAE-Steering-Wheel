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
   `system-architecture-and-can.md` §5 (A1–A9 currently). No board order while an assumption that
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
5a. **"The rail" means the CLAMP voltage, not the nominal voltage, for everything downstream of a TVS.**
   On `+12V_P` the number that matters is **53.3 V**, not 12 V — the clamp is not an abnormal condition,
   it is the designed response to an event we expect. Rule 5 already implied this and was still missed:
   four capacitor lines sat at 50 V on a 53.3 V-clamped rail, one of them annotated "50 V rating
   deliberate for load-dump margin" (defect 8.10), and the buck on the same node had already been
   changed to a 66 V part for exactly this reason (defect 5.1). **After any TVS is chosen, re-walk every
   component on the protected net against the clamp figure.** A rule that is stated but not applied to
   each part in turn is a rule that will be missed.
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
| **G6 — Pre-order** | BOM availability **and lifecycle** re-checked same-day; variants' fitted lists diffed; gerbers visually inspected in a third-party viewer (not Altium); **Q1 source/drain confirmed against the DMP3056L pinout on the drawn schematic** (defect 5.5); **`scripts/check-consistency.py` passes**; **no BOM line still says "confirm", "TBD" or "select at capture"** — placeholder text blocks the gate (L35, which is how defect 8.8 hid) |

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
2a. **Oscillator verification — before trusting any timing.** With the board powered:
   (a) confirm HSE starts, and **repeat at the cold extreme** (`gm_crit` is worst cold; headroom is
   4.4×, not 10×); (b) **measure drive level** — the worst-case estimate is 117 µW against the ABM8's
   100 µW maximum, so this is a real check, not a formality. If over, raise `R_X1` and **re-verify
   cold start**, because `R_X1` trades drive level against startup margin; (c) measure the frequency
   and confirm it is within ±100 ppm — a large error means `C_X1`/`C_X2` are wrong for the stray
   capacitance of this layout. Overdriving a crystal does not fail now; it ages the part and fails
   months later, which is why this is measured rather than assumed.
3. **SWD + option bytes:** Tag-Connect attach, read MCU ID, flash blinky, verify 3.3 V under load.
   **Then set and verify the boot option bytes with STM32CubeProgrammer: `nSWBOOT0` = 0, `nBOOT0` = 1.**
   Without this the board boots to the system bootloader whenever the CAN bus is live (defect 1.3),
   and it cannot be fixed in firmware because boot mode latches during reset. **Re-verify after any
   mass erase** — an erase restores `nSWBOOT0 = 1` and silently re-arms the fault.
3a. **UCPD dead-battery release — do this with the debug adapter ATTACHED** (defect 8.2). Confirm the
   firmware sets `PWR_CR3.UCPD1_DBDIS` before any GPIO setup, then, *with the debug cable plugged in*,
   check the affected pin on each board: **wheel — ENC3 counts cleanly in both directions**;
   **dash — `EVE_INT` reads high while the display idles.** These pins carry a 5.1 kΩ pull-down armed
   by the idle-high debug UART, so this failure is invisible unless the debug harness is connected —
   which is the opposite of every other bug's behaviour. Test in the failing configuration, not the
   convenient one.
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
- **L19 (2026-07, updated):** **Twenty-nine defects so far** — fifteen through Rev B.1 (STM32 pin map ×3, missing clock source, Sharp EXTMODE, TVS-vs-buck abs-max, BAT54S leakage, Riverdi backlight rail, AMS1117 ceramic cap, P-FET orientation, plus the J2 ground allocation caught in review), and fourteen more in the Step 2 design pass (`datasheet-verification.md` §9). The two most expensive (BOOT0-on-CAN, TVS-above-abs-max)
  were both **interactions between two correct-looking choices**, not errors in either one alone.
  PB8 is a fine CAN pin. SMBJ33A is a fine TVS. A 35 V buck is a fine buck. Each fails only in
  combination. **Review pairs, not parts:** for every component, ask what else touches its net and
  what the other end of that net does at power-on, at fault, and when hot.

- **L20 (2026-07, independent audit):** An independent technical review of the *plain-English*
  guides found a **critical error in the source document they were written from** — the P-FET
  reverse-protection orientation was specified backwards in `wheel-schematic-complete.md` while the
  other three source files had it right. Two things follow.
  **(a)** *Explaining* a circuit is a stronger check than reviewing it. The reviewer had to state
  *why* the orientation was correct, and the physics refused to line up — a plain read of the pin
  assignment had passed repeatedly. **Make someone explain the mechanism, not confirm the netlist.**
  **(b)** When one document disagrees with three others, the majority is not automatically right —
  but the disagreement is always worth stopping for. Here the majority *was* right and the
  self-contained file was the outlier, which is the worst case, because that file is the one people
  are told they can build from without opening anything else.
- **L21 (2026-07):** A stale number is a live hazard. The audit found the guides repeating figures
  that were true two revisions ago — a "reflected current" computed for a buck that had become an
  LDO, "provisional" banners on pin maps since verified, a TVS trade-off presented as open after it
  had been closed in the *opposite* direction. Superseded text does not announce itself.
  **When a decision changes, grep for every place the old one is stated — including the prose.**

- **L22 (2026-07, found while writing firmware):** A rule learned from one defect became the blind
  spot for the next. Defect 1.1 taught "encoder pins must be CH1+CH2 of the same timer", and I then
  applied that rule to **TIM15 — a timer with two channels and no quadrature decoder at all**. The
  assignment satisfied the rule and was still wrong. **A checklist item derived from a past failure
  checks that failure, not the category it belongs to.** The general question was "can this
  peripheral do the job", and the specific rule quietly replaced it.
  Second lesson from the same find: **writing the firmware is a design review.** Nothing else forced
  the question "which timers actually have an encoder interface", because on a schematic a timer
  channel is just a pin name.

- **L23 (2026-07, closing open items):** I flagged `nBOOT_SEL` in the "not read" table as *"the one
  that matters, because the whole BOOT0 fix rests on them"* — and then left it unread for several
  work sessions while doing lower-value verification. **When you write down that an item is the
  critical one, that is a queue instruction, not a comment.** Close flagged-critical items first,
  before the easy ones. It was wrong, and the design carried an unusable instruction the whole time.
  Second point: the wrong name came from **family drift** — `nBOOT_SEL` is real, just not on G4.
  Plausible-because-half-remembered is the most dangerous kind of wrong, because it survives a
  reader's sanity check.

- **L24 (2026-07, closing open items):** Two defects in one sitting from the same root — **I had
  recorded pin *types* from memory.** The DAQ analysis assumed a "±5 mA injection budget" that does
  not exist (the spec is −5/0 mA, positive injection not permitted), and the paddle sense taps were
  documented as landing on 5 V-tolerant FT pins when PB0/PB1 are `TT_a` with a **4.0 V** absolute
  maximum. Both readings made an unsafe circuit look safe.
  **Pin electrical class (FT / FT_c / TT / TT_a) is a datasheet fact with a different limit for each,
  and it is invisible in a net name.** Record it next to the pin, like the alternate function.
- **L25 (2026-07):** Sizing against the *nominal* rail instead of the *worst-case* rail. The paddle
  divider was first sized so a 12 V line landed safely — then the arithmetic for a 16 V charging
  system gave 4.35 V against a 4.0 V limit. A "12 V" automotive system is 14.4 V running and higher
  in transient. **Rigor rule 3 says worst case; a rail's *name* is not its worst case.**

- **L26 (2026-07, Step 2 pass):** **A search-and-replace fix is only as good as its weakest format
  match.** The defect-1.5 correction was applied by matching a string in one table's layout; the
  *authoritative* pin table wrote the same fact with different column order, backticks and
  underscores, so it kept the wrong timer and a double-booked pin through several commits. The edit
  reported success. **After any correction, verify the fact is now right everywhere by searching for
  the OLD value, not by confirming the new one appears.** If `TIM15` still returns hits, the fix is
  not finished — regardless of how many places `TIM20` now appears.

- **L27 (2026-07, Step 2 pass):** **"This file is authoritative" is a hazard unless it is enforced
  mechanically.** `board_config.h` correctly said "if this file and the schematic disagree, the
  schematic is right and this file is a bug." Because the schematic's §9 table was the stale one,
  that rule instructed the next engineer to *revert a fix*. A pointer to a source of truth is only
  safe if something checks that the source is actually true. Hence `scripts/check-consistency.py`,
  which is now a commit gate: **prose review cannot detect format-divergent duplication; a parser
  can.** Every defect the script was written to catch, humans had already reviewed past.

- **L28 (2026-07, Step 2 pass):** **A peripheral you never enable can still be wired to your pins.**
  The UCPD dead-battery pull-downs (defect 8.2) are armed by *pin voltage*, not by initialising UCPD
  — so "we don't use USB-PD" was true and completely beside the point. Worse, the trigger was the
  debug UART, the one interface everybody treats as inert, which meant the fault appeared *only while
  being observed*. **For every pin, read the whole alternate-function list including the footnote
  markers, and ask which of those functions are ON BY DEFAULT rather than which you intend to use.**

- **L29 (2026-07, Step 2 pass):** **Verifying that a resource is *reachable* is not verifying its
  *index*.** The audit recorded "all these pins reach ADC1" — true, checked, and useless against
  defect 8.3, where the channel *numbers* were off by one and the 12 V monitor silently read a
  plausible 14.25 V while the 5 V monitor sampled a button. **Write the question down before
  answering it.** "Is PA1 an ADC pin?" and "which channel is PA1?" are different questions, and the
  first one passing feels exactly like the second one passing.

- **L30 (2026-07, Step 2 pass):** **Absence of a rating is not permission.** The paddle pins had no
  positive-injection limit to violate (defect 8.7) — because the datasheet states positive injection
  is *not possible* on those I/Os, i.e. there is **no internal clamp to VDD at all**. The missing
  number was the warning, not the all-clear. When a limit table omits your case, find out whether it
  is omitted because it is safe or because the protecting mechanism does not exist.

- **L31 (2026-07, Step 2 pass):** **Part substitutions must be chased through the passives, not just
  the part number.** Swapping the dash buck LMR33630 → LMR36015 updated the MPN cell and left the old
  inductor/capacitor values behind (defect 8.5), while a sibling document asserted the passives were
  "identical to the wheel". Separately, five components the datasheet calls *required* — C_BOOT,
  C_VCC, both feedback resistors, C_FF — existed only as prose inside a notes cell and were never
  orderable lines; a board built from that BOM would not have switched at all. **A BOM line is a
  procurement promise: if a part is needed to make the circuit function, it gets its own row with a
  quantity, never a mention in someone else's comment field.**

- **L32 (2026-07, Step 2 pass):** **Capture instructions rot faster than the documents they cite,
  because nobody re-reads them until build day.** Two rejected parts (defect 8.4) and one superseded
  circuit (defect 8.6) were still specified as live steps in the Altium instruction files long after
  the BOMs and schematic definitions had moved on. These are the *last* documents a person reads and
  the *first* they act on. **When a decision changes, the step-by-step build instructions are the
  highest-priority place to update, not the lowest** — and say explicitly what NOT to fit, since the
  stale version may already be in someone's head.


- **L33 (2026-07, Step 2 lens 3):** **Walk the power tree forwards; do not read the BOM.** The dash
  3.3 V buck had no bootstrap capacitor (defect 8.8) — the rail would never have come up, taking the
  MCU, the display and the whole analog front end with it. Reading down the BOM, the entry is simply
  not there, and absence is invisible. Walking `+5V → U2 → +3V3` forces the question *"what does U2
  need in order to switch?"*, and the answer comes from the datasheet's recommended-components table,
  where the missing part is a labelled column. **For every active device, enumerate its required
  support parts from its own datasheet table and tick them off against the BOM one at a time.** Two
  converters on two boards had this same class of omission (8.5, 8.8) and every prior review passed
  both.

- **L34 (2026-07, Step 2 lens 3):** **A document that states and denies the same fact is worse than
  one that is simply wrong** — the reader believes whichever half they hit first. The dash power
  budget called the 3.3 V rail a buck in one bullet and an LDO two lines later, and flagged a 0.85 W
  thermal problem that a *different* section had already recorded as closed (defect 8.9). Nothing was
  unsafe; someone's afternoon was. **When an architecture changes, the arithmetic that justified the
  old one has to change with it or be explicitly labelled as the rejected alternative** — keeping the
  comparison is genuinely useful, but only if it says which side is real.

- **L35 (2026-07, Step 2):** **A "confirm this later" note in a shipping document is an unexploded
  defect, not a to-do.** `L2` sat in the dash BOM reading *"CONFIRM against AP63203 datasheet"* across
  every review. Reading it took ten minutes and produced three results at once: the value was fine,
  an open question closed, and **a missing required component surfaced**. The note had been treated as
  a decoration for so long it stopped being read as a request. **Placeholder text must block a gate —
  G6 fails if any BOM line still contains "confirm", "TBD" or "select at capture" without an owner.**

- **L36 (2026-07, audit of the audit):** **A correct conclusion can rest on invented evidence, and the
  correctness is what protects the invention.** §2.2 justified tying the buck's EN pin to VIN by citing
  a datasheet rule — "EN must not exceed VIN by more than 0.3 V" — that **does not exist** (defect
  8.11). The circuit is right; the datasheet sanctions exactly that connection in as many words. But
  the fabricated citation survived every review *because* the design it defended was sound, and it
  reads identically to a verified one. Rule 1a says no number enters a document unless read from a
  datasheet; this is its converse and it needs saying separately: **no *citation* either.** When
  checking a document, verify the evidence against the source, not the design against the evidence —
  those are different activities and only the first catches this.

- **L37 (2026-07):** **A warning that points the wrong way is worse than no warning**, because the
  reader acts on it. Three files said "stocked variants are 1 MHz" about the buck when the stocked part
  is in fact the 400 kHz one (defect 8.12) — so a reader trusting the note buys the wrong variant and
  fits passives sized for the other. The same file said it correctly two hundred lines earlier. **When
  a fact appears in both a body section and a summary/open-items table, the table is the one that goes
  stale**, because closing an item feels like finishing rather than like editing.

- **L38 (2026-07):** **Ask the "what if this part is absent or wrong" question of the parts you just
  added, not only the ones you inherited.** Reversing the BOM found five missing LMR36015 support parts
  (8.5); walking the power tree then found a sixth on the other converter (8.8); an audit then found
  that the check written to prevent recurrence **omitted that sixth part from its own list**. Each pass
  fixed the thing in front of it and left the neighbouring instance. **When a defect has instances,
  enumerate the instances explicitly and tick them off** — "fixed the buck" is not the same as "fixed
  both bucks on both boards and the script that guards them".

- **L39 (2026-07, writing servo.c):** **A rate limit expressed in "per second" and applied at a
  per-millisecond tick is an integer-truncation trap.** 500 µs/s at a 1 ms tick is 0.5 µs, which
  truncates to **zero**, and the servo never moves at all — no error, no alarm, no symptom except an
  ARB that does nothing. I wrote a comment in the header warning about exactly this trap and then
  implemented the trap directly underneath it, which is worth recording on its own: **knowing about a
  failure mode is not the same as having avoided it, and only the test told the difference.** The fix
  is a fractional accumulator carried between ticks, plus a rule that it resets when parked at the
  target so no free jump is banked.

- **L40 (2026-07, writing servo.c tests):** **Three of the four "failures" in the first servo test run
  were the test's fault, and each one was the firmware being right.** The tests commanded a position
  once and then ran the task for four seconds, so rule 1 correctly declared a CAN loss; and they never
  fed feedback, so rule 4 correctly raised a divergence alarm. **Safety rules that fire in your own
  tests are a good sign, but only if you read the failure before "fixing" it.** The tempting response
  — loosening a timeout so the test goes green — would have disabled a rule that was working. Write the
  test to keep the system in the state you mean to test, and treat an unexpected safety trip as
  evidence before treating it as noise.

- **L41 (2026-07, writing daq.c):** **When a datasheet value is unobtainable, ask whether the extreme
  choice is affordable — it often closes the question outright.** The ADC sample-time minimum for a
  5 kΩ source sat marked UNVERIFIED for weeks because it lives in a reference manual ST's server would
  not serve (four attempts). Selecting the hardware's *longest* sample time costs 1% of the scan
  budget and makes the lookup irrelevant: if the maximum is not enough, nothing is, and the fallback
  buffer is required regardless. **An unverified number is a liability that never expires on its own;
  a conservative extreme is a decision that closes.** Check the cost before assuming you have to wait.

- **L42 (2026-07, crystal selection):** **A range is not a specification.** The crystal had sat as
  "8–16 MHz, CL 8–12 pF, ESR ≤ 80 Ω" — which reads like a spec and is actually a set of combinations
  whose startup margin varies by 3×, from 6.5× down to 2.1×. Someone picking the middle of each range
  gets a board that starts unreliably cold. **When a part is specified as ranges, compute the corner
  cases before calling it selected**, and if the corners disagree, the ranges are hiding the real
  constraint. Here the real constraint was a single point: 16 MHz, CL 8 pF.

- **L43 (2026-07, crystal selection):** **"Slower is easier to start" is false for a small crystal.**
  `gm_crit` scales with F², so intuition says drop the frequency — but ESR climbs faster than F² falls
  in a 3225 package (ABM8: 400 Ω at 8 MHz vs 70 Ω at 16 MHz). 8 MHz, the STM32's own reference
  frequency and the obvious "safe" choice, is the **worst** of the four candidates. **Two parameters
  moving in opposite directions need the arithmetic done, not a heuristic** — and the heuristic here
  pointed confidently the wrong way.

- **L44 (2026-07, crystal selection):** **Two independent constraints can pin a value that neither
  pins alone.** The transconductance table says CL = 6 pF; buildability (stray capacitance is ~5 pF,
  so 6 pF CL needs 2 pF external caps) says CL = 10 pF. Only 8 pF satisfies both, and neither
  analysis on its own would have found it. **When a value looks over-determined, check whether you
  have applied every constraint — and when it looks free, you probably have not.**

- **L45 (2026-07, ABM8 datasheet supplied by the user):** **A specification in engineering units is
  not an orderable part, and the gap between them is invisible from inside the design.** Every
  electrical number the crystal analysis rested on was correct — ESR, C0, drive level, aging all
  confirmed to the digit when the datasheet was finally read. What the design had never recorded is
  that on this part **`CL` and operating temperature are order options whose defaults are wrong for
  us**: the standard ABM8 is CL 18 pF and −10…+60 °C, and CL 18 pF drops startup margin from 4.4× to
  **1.2×** (defect 8.14). Nothing internal could have caught this. The spec was self-consistent, the
  physics was right, and a consistency script cannot know a vendor's default differs from the
  assumed value. **Read the ordering-information page, not just the parametric table** — and treat
  "series + electrical spec" as an unfinished selection, however precise the numbers look. The tell
  was already in my own words: the BOM said *"confirm exact option string with the distributor,"* and
  a `[CONFIRM]` note in a shipping document is an open item (L35), not a footnote.

- **L46 (2026-07, same):** **Rank the deviations, or the warning gets discounted wholesale.** Four of
  the ABM8's defaults differ from what this design specifies, and only **two** matter: CL and
  temperature are load-bearing, while the ±50 ppm tolerance/stability defaults still leave 38× CAN
  margin. Flagging all four at equal volume would teach a reader that the warnings are boilerplate —
  and the reader who then skims past CL is the exact failure the warning exists to prevent. **Say
  which items are load-bearing and which are preferences, in the warning itself.**

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
