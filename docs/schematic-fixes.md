# Schematic compile fixes

Punch list from the first full compile of the rebuilt `wheel-power` / `wheel-mcu` / `wheel-io` sheets. Grouped by sheet so it can be worked through in order.

## wheel-power.SchDoc

### 1. Add the V12_SENSE and V5_SENSE dividers

| Net | Components | Values | Wiring |
|---|---|---|---|
| `V12_SENSE` | R_12T, R_12B, C_12F, D_12CL | 43.2kΩ / 10.0kΩ / 100nF / BAT54S-7-F | R_12T: `+12V_P`→node. R_12B: node→`GND`. C_12F: node→`GND`. D_12CL: middle pin→node, one outer→`+3V3A`, other outer→`GND` |
| `V5_SENSE` | R_5T, R_5B, C_5F | 8.2kΩ / 10.0kΩ / 100nF | R_5T: `+5V`→node. R_5B: node→`GND`. C_5F: node→`GND`. No clamp diode needed. |

Label each divider's output tap exactly `V12_SENSE` / `V5_SENSE` — must match `wheel-mcu.SchDoc` character-for-character to connect across sheets.

## wheel-io.SchDoc

### 2. Rename the paddle-down connector's net label

Change **`PADDLE_DOWN`** → **`PADDLE_DN`** on the connector pin. The conditioning circuit (pull-down, TVS, sense tap) already uses `PADDLE_DN` consistently — the one-letter mismatch was the entire problem.

### 3. Add the missing USB_DM label on U5

U5 is a USBLC6-2SC6 ESD array. Pin 4 already correctly carries `USB_DP`. Add label **`USB_DM`** to **pin 6** (its mirror-pair on the MCU-facing side, same row as pin 1).

Pinout reference (from the ST/UTC datasheet):

| Pin | Function |
|---|---|
| 1 | I/O1 (bus side) |
| 2 | GND |
| 3 | I/O2 (bus side) |
| 4 | I/O2 (MCU side) — `USB_DP` |
| 5 | VBUS |
| 6 | I/O1 (MCU side) — needs `USB_DM` |

### 5. Add a debug header (new component — doesn't exist yet)

Place a header with these labeled pins:
- SWD group: `SWDIO`, `SWCLK`, `NRST`, `GND`, `+3V3`
- UART group: `DBG_RX`, `DBG_TX`, `GND`

Can be one combined connector or two separate ones. A 2×5 0.05" ARM SWD header covers the SWD group in the standard form factor if preferred.

## wheel-mcu.SchDoc

### 4. Assign LCD_EXTCOMIN

Label **PA8 (pin 42)** as **`LCD_EXTCOMIN`**. Unused, TIM1_CH1-capable for a hardware-driven toggle output, no JTAG or crystal entanglement.

## Firmware / production (not a schematic change)

### 6. Set option bytes before first field use

STM32G474 defaults to `nSWBOOT0=1` — PB8 (`CAN_RX`) is live as a BOOT0-sense input at every reset out of the box (confirmed against RM0440). In STM32CubeProgrammer's Option Bytes tab, set:
- `nBOOT0` = 1
- `nSWBOOT0` = 0

This forces "always boot from flash," permanently freeing PB8 for CAN_RX with no interaction risk. Do this once per board as part of the programming procedure. Double-check against the specific silicon revision letter before assuming this default applies unchanged.

## No action needed

- **`NetC7_2` no driving source** — this is the FB divider's tap node (R1, R2, C7, U1 pin 7). Passive dividers always trip this warning; it confirms the divider is wired correctly.
- **`+5V` has multiple names** (`+5V_SH`, `+5V_TC`) — traced the actual wires: both are deliberately tied to `+5V` on `wheel-displays.SchDoc`, not an accidental short. Rename to plain `+5V` to clear the warning, or leave it — cosmetic only.
- **CAN_RX mixed pin types** — normal for an MCU RX line; the real substance of this one is the BOOT0 item above.
