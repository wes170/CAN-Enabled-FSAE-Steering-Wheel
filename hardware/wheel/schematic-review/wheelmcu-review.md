# Schematic Review — Sheet `wheelmcu` (STM32G474RET6)

**Part under review:** U3, `STM32G474RET6` — LQFP64, 512 KB flash, 128 KB SRAM, -40…+105 °C (suffix 6).
**Sheet source:** `9f681036-Copy_of_wheelmcu.SchDoc`
**Reviewed against:** DS12288 Rev 6 (STM32G474xB/xC/xE datasheet), AN5093 Rev 2 (STM32G4 hardware
development), AN2867 Rev 24 (oscillator design guide), Abracon ABM8 datasheet (rev 07-29-20),
AVX/Kemet/Yageo passive datasheets. Full list at the end.

> **STATUS: DRAFT IN PROGRESS** — findings below are confirmed against the datasheets cited.
> This banner is removed when the review is complete.

---

## Summary

**Is this sheet fit to build? No.**

The pinout of U3 is transcribed correctly — every one of the 64 pins matches DS12288 Table 12 for
LQFP64 — and most of the peripheral pin assignments are legal per the AF table. But there are
three defects that stop the board working and several that make it unreliable:

1. **CAN_RX is wired to PB8, which is the BOOT0 pin.** On a factory-default STM32G4
   (`nSWBOOT0 = 1`) the boot mode is taken from the PB8 *pin*, not from an option bit. The CAN
   transceiver drives RXD **high** when the bus is idle, so the MCU boots into the system
   bootloader instead of the application — every power-up. The board never runs its firmware.
2. **VREF+ (pin 28) is not connected to anything.** Both ADC sense inputs (V12_SENSE, V5_SENSE)
   are therefore meaningless, and DS12288 Table 14 note 1 requires VREF+ to be connected to a
   supply in the permitted range.
3. **VDDA (pin 29) has no decoupling capacitor anywhere in the design.** It is fed through a
   600 Ω ferrite (FB1, `wheelpower`) into an open circuit. A ferrite with no capacitor after it
   is not a filter; it is a series inductor feeding the analog supply pin.

Additionally, **encoder 3's A and B channels are not connected to the MCU at all** — `ENC3_A` and
`ENC3_B` exist on `wheelhmi` with pull-ups and filters but terminate on that sheet. One of the six
rotary encoders cannot be read.

---

## Findings

| ID | Severity | Finding |
|----|----------|---------|
| MCU-1 | **Stops the board working** | CAN_RX on PB8 = BOOT0; board boots the ROM bootloader, not the application |
| MCU-2 | **Stops the board working** | VREF+ (pin 28) unconnected — ADC has no reference |
| MCU-3 | **Stops the board working** | Encoder 3 quadrature channels (ENC3_A / ENC3_B) never reach the MCU |
| MCU-4 | **Unreliable in service** | VDDA (pin 29) has no 100 nF + 1 µF decoupling; fed through a ferrite into an open circuit |
| MCU-5 | **Unreliable in service** | Crystal Y1 is rated -10 °C to +60 °C — far below the cockpit environment and the MCU's own -40…+105 °C |
| MCU-6 | **Unreliable in service** | PB6/PB7 shorted to PC6/PC7 (ENC2_A/ENC2_B): two MCU pins per net, and PB6 carries a 5.1 kΩ UCPD dead-battery pull-down that drags ENC2_A to an invalid logic level |
| MCU-7 | **Unreliable in service** | VBAT (pin 1) has no dedicated 100 nF, contrary to AN5093 §2.1.3 |
| MCU-8 | Suboptimal | Crystal load capacitors sized without accounting for stray C; CL = 7 pF part is a poor choice at this stray level |
| MCU-9 | Suboptimal | R_X1 = 100 Ω series resistor has no design basis — drive level does not require it, and it costs oscillator gain margin |
| MCU-10 | Suboptimal | PC3 shorted to PB13 (LCD_DISP) — two GPIOs tied together |
| MCU-11 | Suboptimal | No VBUS sense to the MCU; the D4/R_VBUS1 network on `wheelio` loads USB_DP for no function |
| MCU-12 | Suboptimal | Symbol/annotation defects: R? unannotated, PA14 declared as an Output pin |
| MCU-13 | Suboptimal | SWO (PB3) sacrificed to ENC4_B and loaded with 100 nF — no trace output possible |

*(sections, pin-by-pin table, verified-correct list and datasheet list follow — in progress)*
