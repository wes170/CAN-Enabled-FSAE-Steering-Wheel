# Project Log — gate sign-offs and assumption tracker

`memory/engineering-rigor.md` says gates are "signed & dated in the project log" and assumptions are
"tracked, numbered, and closed." This is that log. **Nothing here is ticked by whoever did the work
alone** — G5 exists specifically so a second person looks.

Keep this file honest. A gate marked passed that wasn't is worse than no gate, because it silently
removes the check everyone downstream assumes happened.

---

## 1. Open assumptions (from `memory/system-architecture-and-can.md` §5)

Close these before trusting the design. **A1 and A2 are closable for the price of a ~$30 USB-CAN
sniffer, with no PCB spend** — do them first, because they are the two that could force a respin.
~~A6 must be closed before dash layout~~ — **closed 2026-07: standard PWM servos confirmed**, so the
dash output stage is correct as drawn and layout is unblocked. Only BEC sizing remains under A6.

| # | Assumption | Blocks | How to close | Status | Closed by / date |
|---|---|---|---|---|---|
| A1 | NSP detects an emulated keypad at node `0x15`, 1 Mbit/s | Wheel firmware | Sniff a real Haltech keypad / NSP discovery on bench CAN | **OPEN — deferred by choice** (user, 2026-07): not a blocker for schematic/layout work. Kept as a one-line constant so it is cheap to change | |
| A2 | IO12 **Box B** CAN ID set (Box A verified) | Dash DAQ re-broadcast | Request write protocol from Haltech support (supplied to owners), or sniff a Box B | **OPEN — deferred by choice** (user, 2026-07) | |
| A3 | WS2812B-2020 worst case 36 mA/LED | Wheel power budget | Measure a 24-LED strip at full white | OPEN | |
| A4 | TC / lockup status source on the broadcast bus | Wheel LED firmware | Decide with the tuner in NSP; bind the config table | OPEN | |
| A5 | Riverdi 3.3 V backlight inrush | Dash 3V3 buck | Scope at power-on; verify AP63203 soft-start covers it | OPEN | |
| A6 | ARB servo **stall current** (assumed ~5 A @ 7.4 V/ch) | **BEC sizing only — no longer blocks dash layout** | Get the part number from vehicle dynamics | **PARTIALLY CLOSED** — user confirmed **standard PWM servos** (2026-07), so the dash output stage (TIM4 → 74AHCT2G125 → 100 Ω → SMAJ5.0A) is correct as drawn. Only the harness BEC sizing remains | PWM confirmed 2026-07 |
| A7 | Does the ARB mechanism back-drive on power loss? | ARB safety strategy | Mechanical team. Worm drive holds; direct lever does not | OPEN | |
| A8 | Wheel 12 V transient environment (now seen raw) | Wheel input protection | Scope the feed during crank, alternator steps, fan/solenoid switching | OPEN | |
| A9 | Wheel colour display rated −20…+70 °C, tighter than the rest of the BOM | Wheel display longevity | Measure panel surface temperature on a summer track day; faceplate shade/recess if exceeded | OPEN | |

---

## 2. Review gates

Run in order, per board. Evidence means an artifact someone else can re-check (a screenshot, a
report file, a photo, a scope capture) — not a memory of having looked.

### FSAE-WHEEL Rev B

| Gate | Content | Evidence | Passed | Signed / date |
|---|---|---|---|---|
| G1 | Schematic review — ERC clean, suppressions justified, pin maps match docs, power budget re-added, every connector pin accounted for | | ☐ | |
| G2 | Layout review — DRC clean, 3D collision vs. faceplate STEP, L2 GND unbroken, protection-at-connector audit, buck hot-loop audit | | ☐ | |
| G3 | Paper build — 1:1 print on the real wheel, gloved reach test for every encoder and button | | ☐ | |
| G4 | Netlist cross-check — J1 pinout re-derived independently, **including which ground each signal references** (L13) | | ☐ | |
| G5 | Peer sign-off — second person reviews G1–G4 evidence | | ☐ | |
| G6 | Pre-order — BOM availability **and lifecycle** re-checked same-day (rule 3a), variant fitted-lists diffed, gerbers viewed in a non-Altium viewer | | ☐ | |

### FSAE-DASH Rev B

| Gate | Content | Evidence | Passed | Signed / date |
|---|---|---|---|---|
| G1 | Schematic review (as above). A6's layout risk is closed — PWM servos confirmed | | ☐ | |
| G2 | Layout review (+ LMR36015 hot loop, analog zone separation from the servo PWM traces) | | ☐ | |
| G3 | Paper build — 1:1 print in the real dash panel, FPC fold mocked in paper | | ☐ | |
| G4 | Netlist cross-check — J1 **and** J2 pinouts, and the servo ground reference path | | ☐ | |
| G5 | Peer sign-off | | ☐ | |
| G6 | Pre-order | | ☐ | |

---

## 3. Procurement actions (do these before PCB spend)

| Item | Why it is first | Status |
|---|---|---|
| **Riverdi RVT50HQBNWN00 + 1 spare** | Single-source, thin distributor stock, longest lead in the BOM. Nothing about the PCB design depends on ordering boards first, so there is no reason to wait | ☐ |
| USB-CAN sniffer (~$30) | Closes A1 and A2 without spending board money | ☐ |
| Encoders (PEC09 + PEC11H) | Confirm shaft length against the real faceplate depth before the footprint is frozen | ☐ |
| **JDI LPM013M126A wheel display + 1 spare** | Specialty distributors only (Switch-Science / Data Modul / Youritech), not Digi-Key or LCSC — thinner supply than the rest of the BOM. Sharp LS013B7DH05 is the zero-board-change mono fallback if it fails | ☐ |
| Haltech IO12 Box B write protocol | Email Haltech support with proof of ownership (they supply it) — closes A2 | ☐ |

---

## 4. Revision history

| Rev | Date | Change |
|---|---|---|
| A | 2026-07 | Initial design. Wheel fed 5 V from the dash |
| B | 2026-07 | Wheel moved to vehicle 12 V and made standalone; dash 5 V AUX deleted and 5 V buck downsized to 2 A; 2× ARB servo outputs added to the dash on J1.5/J1.6; obsolete EVQ-WK4001 thumb encoder replaced with the Bourns PEC09 right-angle, plus a DNP satellite-board option |
| B.1 | 2026-07 | Wheel display changed to colour (JDI LPM013M126A, 176×176 8-colour reflective MIP) — pin-for-pin identical to the Sharp mono part, so a BOM change only; Sharp retained as a zero-board-change fallback. Adds assumption A9 |
| B.2 | 2026-07 | **Step 2 full design pass — seven defects** (`datasheet-verification.md` §9). Board changes: **`D14`/`D15` BAV199 clamps added** to the paddle sense nets (8.7 — the divider alone let a TVS clamp put 8 V on a 4.0 V pin), and both BOMs gained the LMR36015's datasheet-required `C_BOOT`, `C_VCC`, `R_FBT`, `R_FBB`, `C_FF` plus the mandatory 2 × 220 nF VIN bypass caps, which had existed only as prose (8.5); the dash buck's input/output capacitors were still the rejected LMR33630's values. Firmware requirements: `PWR_CR3.UCPD1_DBDIS` must be set before any GPIO setup or the debug UART disables `ENC3_A` (8.2), and the wheel rail-monitor ADC channels were off by one (8.3). Documentation: the authoritative §9 pin table was stale (8.1) and both Altium instruction files still named rejected power parts (8.4, 8.6). Adds `scripts/check-consistency.py` as a commit gate |
