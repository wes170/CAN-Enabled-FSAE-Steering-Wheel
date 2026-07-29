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
**A6 must be closed before dash layout**, because a serial-bus servo changes the output stage from
PWM to half-duplex UART.

| # | Assumption | Blocks | How to close | Status | Closed by / date |
|---|---|---|---|---|---|
| A1 | NSP detects an emulated keypad at node `0x15`, 1 Mbit/s | Wheel firmware | Sniff a real Haltech keypad / NSP discovery on bench CAN | OPEN | |
| A2 | IO12 **Box B** CAN ID set (Box A verified) | Dash DAQ re-broadcast | Request write protocol from Haltech support (supplied to owners), or sniff a Box B | OPEN | |
| A3 | WS2812B-2020 worst case 36 mA/LED | Wheel power budget | Measure a 24-LED strip at full white | OPEN | |
| A4 | TC / lockup status source on the broadcast bus | Wheel LED firmware | Decide with the tuner in NSP; bind the config table | OPEN | |
| A5 | Riverdi 3.3 V backlight inrush | Dash 3V3 buck | Scope at power-on; verify AP63203 soft-start covers it | OPEN | |
| A6 | ARB servo torque / stall current (assumed ~5 A @ 7.4 V/ch) | **Dash layout** + BEC sizing | Get the part number from vehicle dynamics. **If serial-bus servos, the output stage changes** | OPEN | |
| A7 | Does the ARB mechanism back-drive on power loss? | ARB safety strategy | Mechanical team. Worm drive holds; direct lever does not | OPEN | |
| A8 | Wheel 12 V transient environment (now seen raw) | Wheel input protection | Scope the feed during crank, alternator steps, fan/solenoid switching | OPEN | |

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
| G1 | Schematic review (as above; **A6 must be closed first** — it can change the servo output stage) | | ☐ | |
| G2 | Layout review (+ LMR33630 hot loop, analog zone separation from the servo PWM traces) | | ☐ | |
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
| Haltech IO12 Box B write protocol | Email Haltech support with proof of ownership (they supply it) — closes A2 | ☐ |

---

## 4. Revision history

| Rev | Date | Change |
|---|---|---|
| A | 2026-07 | Initial design. Wheel fed 5 V from the dash |
| B | 2026-07 | Wheel moved to vehicle 12 V and made standalone; dash 5 V AUX deleted and 5 V buck downsized to 2 A; 2× ARB servo outputs added to the dash on J1.5/J1.6; obsolete EVQ-WK4001 thumb encoder replaced with the Bourns PEC09 right-angle, plus a DNP satellite-board option |
