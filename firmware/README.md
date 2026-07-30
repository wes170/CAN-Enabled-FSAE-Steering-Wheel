# `firmware/` — one codebase, two boards

Both the wheel and the dash run the same STM32G474 firmware, selected at compile
time with `-DBOARD_WHEEL` or `-DBOARD_DASH`. That is the whole reason the two boards share an MCU:
one toolchain, one CAN stack, one set of bugs to fix.

```
firmware/
├── include/
│   ├── board_config.h     pin map for both boards — the single source of truth
│   ├── clock_config.h     PLL/CAN timing, with datasheet limits as static asserts
│   ├── system_init.h
│   └── haltech_can.h      keypad emulation, IO12 emulation, broadcast decode
├── src/
│   ├── system_init.c      clock tree, UCPD dead-battery release, boot guard
│   └── haltech_can.c      the three protocols, hardware-free
└── test/
    └── run-tests.sh       host tests — no board, no cross-compiler needed
```

## Running the tests

```sh
./firmware/test/run-tests.sh
```

Compiles every source file for **both** `-DBOARD_WHEEL` and `-DBOARD_DASH` under
`-Werror` (a change that only builds for one board breaks the other silently), then runs the logic
that can be proven at a desk. Register writes compile out via `FIRMWARE_HOST_BUILD`.

The protocol tests are the valuable ones, because every failure they check for is **silent on real
hardware**: a keypad that transmits before NMT start is simply ignored, a byte-order slip produces
plausible numbers, and a stale reading looks exactly like a live one.

## Status: foundation only

What exists is the part that could be written **correctly** today — the pin
configuration and the CAN protocol layer, both derived from verified documents rather than from
memory. What does not exist yet is everything that needs hardware to test against.

| Piece | State |
|---|---|
| `board_config.h` | Complete, from the verified pin maps |
| `clock_config.h` | Complete — PLL values checked against DS12288 Table 46 by static assertion |
| `haltech_can.h` | Complete interface; frame layouts transcribed from primary sources |
| `system_init.c` | Written and host-tested (clock tree, UCPD release, boot guard) |
| `haltech_can.c` | Written and host-tested (keypad, IO12, broadcast decode) |
| `encoder.c` | Written and host-tested (quadrature decode, detents, end-stops) |
| `input.c` | Written and host-tested (integrating debounce with hysteresis) |
| `led.c` | Written and host-tested (WS2812 DMA + the global current cap) |
| `servo.c` | Written and host-tested (all four ARB safety rules) |
| `daq.c` | Written and host-tested (8-channel scan, conversions, validity) |
| `display_wheel.c` | Written and host-tested (JDI protocol, EXTCOMIN watchdog) |
| `display_dash.c` (BT817 EVE) | Not written |
| USB HID (sim variant) | Not written |

## Writing the firmware already found a hardware defect

This is worth reading before you continue, because it is the second time this has happened and it
will happen again.

**ENC5 was assigned to TIM15, which cannot decode quadrature encoders.** TIM15 has two channels, so
it satisfied the rule established earlier ("encoder pins must be CH1 and CH2 of the same timer") and
looked completely correct on the schematic. But the STM32G474 datasheet only claims quadrature
support for TIM1/2/3/4/5/8/20 and LPTIM1 — TIM15/16/17 are described with input capture, output
compare, PWM and one-pulse mode, and no encoder interface.

Nothing about drawing a schematic forces you to ask "can this peripheral actually do the job?"
On a schematic, a timer channel is just a pin name. Configuring the timer is what asks the question.
ENC5 moved to **PB2/PC2 on TIM20**, and `LCD_DISP` moved to PB13 to free `TIM20_CH2`.
Full write-up: `memory/datasheet-verification.md` defect 1.5.

## Rules for this codebase

1. **`board_config.h` is downstream of the schematic definitions, never upstream.** If it disagrees
   with `memory/wheel-schematic-complete.md` §9 or `memory/dash-schematic-complete.md` §8, the
   schematic is right and this file is the bug. Do not reconcile in the wrong direction.

   ⚠ **But do not apply that rule blindly.** Defect 8.1 was exactly this: §9 was the *stale* document,
   and following this rule literally would have reverted a correct fix and reinstated defect 1.5.
   A pointer to a source of truth is only safe if something checks the source is actually true.
   **Before reconciling any mismatch, run `python3 scripts/check-consistency.py`** — it parses the pin
   table for double-booked pins, encoder timers that cannot decode quadrature, and ADC channel numbers
   against the datasheet. If the script is clean and you still disagree with it, go to the datasheet,
   not to either file.
2. **Anything protocol-shaped is a constant, not a literal in logic.** Node IDs, CAN IDs, scaling,
   LED thresholds. Assumption A1 (the keypad node ID) is the most likely thing to be wrong on first
   bring-up; it must be a one-line change, not a hunt.
3. **Safety behaviours are not tunable.** The ARB servo rules and the stale-data indicators are
   requirements with reasons written next to them in the headers. Read the reason before changing
   the value.
4. **Uncertainty is marked in the code.** Where a figure came from memory rather than a datasheet it
   says so inline. Do not quietly promote those to fact — but do ask whether a conservative extreme
   closes the question instead of waiting. `AIN_SAMPLE_CYCLES` was resolved that way (lesson L41), and
   the two remaining markers are in `system_init.c`, both bench-verifiable and both loud on failure.

## Before any of this runs on hardware

Close **A1** (keypad node ID) and **A2** (IO12 Box B frame IDs) with a USB-CAN sniffer. Both are
protocol assumptions, both are cheap to close, and both would otherwise be discovered the hard way
with a board in your hand and an ECU that ignores it.
