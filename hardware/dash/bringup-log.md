# Bring-up log — FSAE-DASH

Procedure: `memory/engineering-rigor.md` §4, plus the dash-specific steps in
`memory/dash-pcb-altium-instructions.md` §3. **Rule 9: one change at a time, and write down what you
saw.**

---

## Board #___  ·  built ____-__-__

| # | Stage | Expected | Observed | Pass | Date / initials |
|---|---|---|---|---|---|
| 1 | Visual + meter | Solder inspected; GND continuity; rail-to-GND > 100 Ω **before first power** | | ☐ | |
| 2 | Power-only | 12 V in, limited to 200 mA. `+12V_P`, `+5V`, `+3V3` within ±3%; thermal sweep on both bucks | | ☐ | |
| 3 | SWD | MCU ID reads, blinky, 3.3 V under load | | ☐ | |
| 4 | CAN loopback | Loopback, then bench bus at 1 Mbit/s, sample point ≈ 80% | | ☐ | |
| 5 | Protocol | Haltech broadcast decode matches known ECU values; stale-data indicator appears after 500 ms of silence (**this is a requirement — verify it, don't assume it**) | | ☐ | |
| 6 | DAQ | Inject known voltages 0 / 2.5 / 5 V on all 8 channels; check linearity and cross-talk; verify a 12 V fault on one channel harms nothing | | ☐ | |
| 7 | Display | **Dummy-load the 3V3 rail first** (3.3 Ω) and scope inrush before connecting the real module (**closes A5**). Then sunlight test at full brightness *behind the actual lens* (L7) | | ☐ | |
| 8 | ARB servos | Five-step bench procedure below — **servos off the car, linkage disconnected** | | ☐ | |
| 9 | Environment | 1 h soak at 60 °C; vibration while logging CAN | | ☐ | |
| 10 | In-car | Full harness; tuner session | | ☐ | |

### Stage 8 detail — ARB servo bring-up
The order matters: prove the software limits before the hardware can hurt itself.

| # | Check | Observed | Pass |
|---|---|---|---|
| 8.1 | Scope both PWM outputs **before any servo is connected** — 1.0 / 1.5 / 2.0 ms pulses, clean 5 V edges | | ☐ |
| 8.2 | Firmware endstops clamp out-of-range commands — verified **before the servo is bolted to a linkage**. A servo grinding a hard stop is the most likely way to destroy one | | ☐ |
| 8.3 | Servo BEC ground starred to dash GND — measure ground-to-ground offset under servo load. Any significant offset means the PWM reference is wrong and pulse widths will jitter | | ☐ |
| 8.4 | CAN-loss behaviour — unplug the wheel mid-adjustment. The servo must **hold**, not centre | | ☐ |
| 8.5 | Divergence alarm — stall the output by hand at low travel; dash must flag and log | | ☐ |

### Observations, surprises, and rework
> Including the fast fixes. Those are the ones that repeat on the next board if nobody wrote them down.

-
