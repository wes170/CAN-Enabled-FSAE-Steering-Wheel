# Bring-up log — FSAE-WHEEL

Procedure: `memory/engineering-rigor.md` §4. **Rule 9: change one thing at a time, and write down
what you observed — including the things that looked fine.** A log with only failures in it is a log
someone started keeping halfway through.

Copy the block below per board built. Record the board serial (write it on the PCB in marker).

---

## Board #___  ·  built ____-__-__  ·  variant: CAR / SIM

| # | Stage | Expected | Observed | Pass | Date / initials |
|---|---|---|---|---|---|
| 1 | Visual + meter | Solder inspected under magnification; all GNDs continuous; every rail-to-GND > 100 Ω **before first power** | | ☐ | |
| 2 | Power-only | 12 V in, current-limited to 150 mA. `+12V_P`, `+5V`, `+3V3` all within ±3%. Thermal sweep — watch the buck inductor | | ☐ | |
| 3 | SWD | Tag-Connect attaches, MCU ID reads, blinky runs, 3.3 V holds under load | | ☐ | |
| 4 | CAN loopback | FDCAN external loopback OK; two-node bench bus at 1 Mbit/s; scope sample point ≈ 80% | | ☐ | |
| 5 | Protocol close-out | Emulated keypad detected by NSP (**closes A1**); IO12 AVI values visible in NSP; broadcast decode matches known ECU values | | ☐ | |
| 6 | HMI | Every encoder ×20 detents CW and CCW, slow and flicked, zero miscounts; every switch ×100; paddle sense voltages correct in both states | | ☐ | |
| 7 | LEDs / display | Measure current at 100% white (**closes A3**); outdoor sunlight legibility check on the LCD | | ☐ | |
| 9 | Environment | 1 h soak at 60 °C running; vibration (shaker, or the real car at idle + rev sweeps) while logging CAN for dropouts | | ☐ | |
| 10 | In-car | Full harness; mapping session with the tuner; gloves-on usability pass | | ☐ | |

*(Stage 8 is servo bring-up — dash only, not applicable to this board.)*

### Transient survey (closes A8)
Scope the 12 V feed at J1 during: engine crank, alternator load steps, fan switching, solenoid
switching. Record the worst excursion and compare against the SMBJ33A clamp and the AP63205 abs-max.

| Event | Worst excursion (V) | Clamp adequate? | Notes |
|---|---|---|---|
| Crank | | | |
| Alternator load step | | | |
| Fan / solenoid switching | | | |

### Observations, surprises, and rework
> Anything that did not match expectation goes here, even if you fixed it in thirty seconds. This is
> the raw material for the lessons-learned file — and the fast fixes are the ones that get forgotten
> and then repeat on the next board.

-
