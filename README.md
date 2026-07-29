# CAN-Enabled FSAE Steering Wheel

FSAE steering wheel + dash + sim-rig variant, built on one STM32G474 platform, talking to a
**Haltech Nexus R5** at 1 Mbit/s CAN. The wheel needs only **5V, GND, paddle-up, paddle-down, and CAN**.

## Permanent memory files (`memory/`)

Read in this order:

| File | Purpose |
|---|---|
| [hardware-selections.md](memory/hardware-selections.md) | Every part selected, with first-principles justification and availability |
| [system-architecture-and-can.md](memory/system-architecture-and-can.md) | Topology, power budgets (shown work), Haltech keypad/IO12 emulation + broadcast protocol, firmware notes, open assumptions A1–A5 |
| [wheel-pcb-altium-instructions.md](memory/wheel-pcb-altium-instructions.md) | Step-by-step Altium workflow for the wheel PCB, through JLCPCB PCBA outputs |
| [dash-pcb-altium-instructions.md](memory/dash-pcb-altium-instructions.md) | Dash deltas: 12V automotive entry, DAQ analog front end, 1000-nit EVE display |
| [sim-variant-instructions.md](memory/sim-variant-instructions.md) | Sim-rig build as an Altium assembly variant + USB HID firmware notes |
| [engineering-rigor.md](memory/engineering-rigor.md) | Standing rules, review gates G1–G6, staged bring-up, lessons learned (append-only) |

## System in one paragraph

The wheel impersonates Haltech's own CAN devices so NSP auto-detects it: buttons ride a CANopen
**keypad emulation** (Blink PKP protocol), encoder positions become synthetic rotary-trim voltages via
**IO12 expander emulation** (AVI channels), and the ECU's LED commands come back to drive button
lighting. Both wheel and dash decode the **Haltech broadcast protocol** (RPM → 16-LED shift bar,
temps/pressures → 5" 1000-nit dash). The dash also provides 8 protected 0–5V DAQ analog inputs and
the fused 5V feed that powers the wheel. The sim variant is the same wheel PCB with USB-C populated,
CAN unpopulated, enumerating as a USB HID gamepad.
