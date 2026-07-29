# Datasheet Verification Record

> **Permanent memory file.** Created 2026-07 after an audit question exposed that the original
> schematic instructions were written largely **from memory rather than from datasheets**. This file
> records, per part: what the datasheet actually says, whether the design complies, and what was
> corrected. A part is not `VERIFIED` until someone has read its datasheet and written the finding here.
>
> **Status legend:** `VERIFIED` = datasheet read, design checked · `PARTIAL` = some parameters checked ·
> `UNVERIFIED` = not yet read, treat every claim about it as provisional.

## Summary of status

| Part | Status | Outcome |
|---|---|---|
| STM32G474RET6 | **VERIFIED** (AF/pin tables) | **3 defects found — pin map rebuilt.** See §1 |
| Sharp LS013B7DH05 | **PARTIAL** | **1 defect found (EXTMODE omitted).** See §2 |
| TJA1051T/3 | **VERIFIED** | Compliant as designed. See §3 |
| Haltech CAN broadcast protocol | VERIFIED | Read in full; §2.3 table transcribed from it |
| Blink PKP2600SI CANopen | VERIFIED | Read in full; keypad frames transcribed from it |
| WS2812B-2020 | **UNVERIFIED** | Datasheet is image-only, not text-extractable. See §4 |
| AP63205 / AP63203 / LMR33630 / AMS1117 | **UNVERIFIED** | All passive values still from memory. See §5 |
| DMP3056L, PESD2CAN, USBLC6, BAT54S, BAV99, SMBJ/SMAJ | **UNVERIFIED** | §5 |
| PEC09 / PEC11H / KSC4 | **PARTIAL** | Distributor parametric data only, not datasheets |
| Riverdi RVT50HQBNWN00 | **UNVERIFIED** | Pinout and the 1.2 A backlight figure are both unconfirmed |

---

## 1. STM32G474RET6 — VERIFIED, three defects found

Source: ST datasheet **DS12288 / stm32g474cb.pdf**, Table 13 (Alternate function) and the pin
definition table, extracted and read directly.

### Defect 1.1 — Only 2 of 6 encoder pairs could have worked (CRITICAL)

STM32 hardware quadrature decoding requires **TIMx_CH1 *and* TIMx_CH2 of the same timer**. The
original map assigned arbitrary adjacent GPIO pairs and claimed "TIM encoder mode ×4". Checked
against the real AF table:

| Original pair | Reality | Verdict |
|---|---|---|
| PC0 / PC1 | TIM1_CH1 / TIM1_CH2 | ✅ valid (by luck) |
| PC2 / PC3 | TIM1_**CH3** / TIM1_**CH4** | ❌ encoder mode uses CH1/CH2 only |
| PC4 / PC5 | TIM1_ETR / TIM15_BKIN | ❌ not a channel pair at all |
| PC6 / PC7 | TIM3_CH1 / TIM3_CH2 | ✅ valid |
| PB0 / PB1 | TIM3_CH3 / TIM3_CH4 | ❌ wrong channels, and TIM3 already taken |
| PB2 / PB10 | TIM5_CH1 / TIM2_CH3 | ❌ different timers |

Four of six encoders would have had no hardware decoder. On a wheel where a flicked encoder is
exactly the case you must not miss counts on, that is a functional failure, not a nuisance.

### Defect 1.2 — LED data pin collided with an encoder timer

`LED_DATA` was assigned **PA8 = TIM1_CH1**, while encoder 1 used **PC0 = TIM1_CH1**. The same timer
channel cannot do both. One of the two would have been silently dropped at CubeMX configuration.

### Defect 1.3 — CAN_RX sits on the BOOT0 pin (CRITICAL, and unavoidable on this package)

`FDCAN1` on LQFP-64 is available **only** on `PA11/PA12` or `PB8/PB9` (PD0's FDCAN1_RX is not bonded
on LQFP-64 — confirmed "–" in the LQFP-64 column). PA11/PA12 are `USB_DM`/`USB_DP`, which the sim
variant and DFU need. **Therefore CAN must use PB8/PB9 — and PB8 is `PB8-BOOT0`.**

Why this matters: an idle CAN bus is **recessive = RXD HIGH**. If BOOT0 is taken from the pin, the
MCU samples BOOT0 = 1 at every power-on with a live bus and jumps to the system bootloader instead of
the application. The wheel would simply never start in the car, while working perfectly on the bench
with the bus unplugged — one of the nastiest failure signatures possible.

**Required mitigation (must be in the build procedure, not just the schematic):**
1. Set the option bits so BOOT0 comes from the **`nBOOT0` option bit, not the pin** (`nBOOT_SEL = 1`),
   as part of first flashing. Verify it on every board.
2. **Delete the "10 kΩ BOOT0 pulldown + test point" from the original instructions** — that circuit
   fights the CAN transceiver's RXD output and is actively harmful here.
3. DFU entry uses the USB DFU path or an SWD-triggered jump, **not** a BOOT0 strap.
4. A full chip erase can restore the factory option-bit state, so **re-check `nBOOT_SEL` after any
   mass erase.** Put it in the bring-up log.

### Corrected wheel pin map — every entry justified by the AF table

| Function | Pin | Justification (from Table 13) |
|---|---|---|
| ENC1 A / B | PC0 / PC1 | TIM1_CH1 / TIM1_CH2 |
| ENC2 A / B | PC6 / PC7 | TIM3_CH1 / TIM3_CH2 |
| ENC3 A / B | PB6 / PB7 | TIM4_CH1 / TIM4_CH2 |
| ENC4 A / B | PA15 / PB3 | TIM2_CH1 / TIM2_CH2 |
| ENC5 A / B | PB14 / PB15 | TIM15_CH1 / TIM15_CH2 |
| ENC6 A / B | PA0 / PC12 | TIM5_CH1 / TIM5_CH2 |
| LED_DATA | PA6 | TIM16_CH1 + DMA — moved off TIM1 (defect 1.2) |
| LCD_SCLK / LCD_SI | PA5 / PA7 | SPI1_SCK / SPI1_MOSI (PB3 alternative is taken by ENC4) |
| LCD_SCS / LCD_DISP / LCD_EXTCOMIN | PA4 / PC2 / PC3 | plain GPIO; EXTCOMIN is a ~1 Hz software toggle, no timer needed |
| CAN_RX / CAN_TX | PB8 / PB9 | FDCAN1 — **only option left after USB claims PA11/PA12** (defect 1.3) |
| USB_DM / USB_DP | PA11 / PA12 | USB FS |
| SWDIO / SWCLK | PA13 / PA14 | |
| DBG_TX / DBG_RX | PA9 / PA10 | USART1 — free now that TIM1 encoder moved to PC0/PC1 |
| V12_SENSE / V5_SENSE | PA1 / PA2 | ADC1_IN3 / ADC1_IN4 |
| PADDLE_UP/DN_SNS | PB0 / PB1 | GPIO input |
| ENC1–6_SW | PC4, PC5, PC8, PC9, PC10, PC11 | GPIO input |
| BTN1–6 | PC13, PD2, PA3, PB10, PB11, PB12 | GPIO input (PD2 = pin 55, confirmed bonded on LQFP-64) |
| Spare | PA8, PB2, PB4, PB5, PB13 | |

Verified bonded on LQFP-64 (pin numbers from the pin definition table): PC13=2, PC0=8, PC1=9,
PC2=10, PC3=11, PC4=22, PC5=23, PB14=36, PB15=37, PC6=38, PC7=39, PC8=40, PC9=41, PA8=42, PA9=43,
PA15=51, PC10=52, PC11=53, PC12=54, PD2=55, PB3=56, PB4=57, PB5=58.

**Dash pin map has the same class of problems and has NOT yet been rebuilt** — it inherits the
CAN/BOOT0 issue (defect 1.3 applies identically), and its `PB6/PB7` servo assignment now collides
with the wheel's ENC3, which is fine across boards but must be re-checked against the shared
firmware's board-personality gating.

---

## 2. Sharp LS013B7DH05 — PARTIAL, one defect found

Source: Sharp spec sheet **LD-27503A**, §4 Table 4-1, read directly.

**Defect 2.1 — `EXTMODE` (pin 8) was omitted from the schematic instructions.** The real terminal
list is 10 pins: 1 SCLK, 2 SI, 3 SCS, 4 EXTCOMIN, 5 DISP, 6 VDDA, 7 VDD, 8 **EXTMODE**, 9 VSS,
10 VSSA. The original instruction listed seven signals and never mentioned EXTMODE.

EXTMODE selects the COM-inversion source: `Hi` enables the hardware EXTCOMIN pin, `Lo` uses the
software serial flag. COM inversion is what prevents a DC bias accumulating across the liquid
crystal, so leaving pin 8 floating gives undefined inversion behaviour and risks permanent image
sticking. **Corrected:** strap EXTMODE to `+3V3` via a 0 Ω link (DNP pulldown alternative), matching
the hardware-EXTCOMIN scheme the firmware uses. Already applied to
`wheel-pcb-altium-instructions.md` §3.6.

Also confirmed from the spec: VDD and VDDA are separate supplies with separate returns (VSS/VSSA),
and the spec explicitly requires low VDD–GND / VDDA–GND impedance in use, which justifies the
decoupling at the connector.

**Still unverified on this part:** FPC contact side (top vs bottom — a mirrored pinout if wrong),
SCS polarity and setup/hold timing, VDD voltage range, EXTCOMIN frequency requirement.

---

## 3. TJA1051T/3 — VERIFIED, compliant

Source: NXP TJA1051 datasheet, pin description table and characteristics, read directly.

| Datasheet says | Design does | Verdict |
|---|---|---|
| Pin 1 TXD, 2 GND, 3 VCC, 4 RXD, **5 VIO** (T/3 and TK/3 only), 6 CANL, 7 CANH, **8 S** | Matches | ✅ |
| VCC = 4.5–5.5 V | 5 V rail | ✅ |
| VIO = 2.8–5.5 V | 3.3 V | ✅ |
| Pin 8 S = Silent mode control input | Tied to GND for normal mode | ✅ |

The `/3` suffix genuinely is the VIO variant, so the level-shift-free interface to the G474 works as
claimed. No changes needed.

---

## 4. WS2812B-2020 — UNVERIFIED, blocked

The World Semi datasheet PDF is **image-only** (scanned/rendered artwork, no text layer), so the
parameters could not be extracted. Unconfirmed claims currently in the docs: 36 mA/LED worst case,
VIH = 0.7·VDD, −40…+85 °C rating, and the timing that the DMA driver will depend on.

**This is already the right shape of risk** — assumption **A3** exists precisely to measure the
current rather than trust it. Resolve by: OCR'ing the datasheet, obtaining a text version from
World Semi, or measuring on the bench. The 74AHCT1G125 level-shift decision does not depend on the
exact VIH number: with a 5 V rail the threshold is high enough that 3.3 V logic is marginal under any
plausible reading, so the buffer stays regardless.

---

## 5. Everything else — UNVERIFIED

Not yet read, and every parameter quoted for them in the other memory files should be treated as
provisional:

- **Regulators** (AP63205WU-7, AP63203WU-7, LMR33630ADDAR, AMS1117-3.3): output current, input
  range, absolute maximum, and **all** inductor/capacitor values. The BOMs already say "confirm
  against the exact datasheet at capture" — that instruction is load-bearing, not boilerplate.
- **Protection** (DMP3056L, PESD2CAN, USBLC6-2SC6, BAT54S, BAV99, SMBJ33A, SMBJ5.0A, SMAJ24CA,
  SMAJ5.0A): standoff and clamping voltages. **The open question from the start — whether the
  SMBJ33A's clamping voltage stays under the LMR33630's 42 V absolute maximum — is still open**, and
  it is a real one, because a TVS that clamps above the buck's abs-max protects nothing.
- **HMI** (PEC09, PEC11H, KSC4): only distributor parametric data was read. Contact bounce duration,
  switch current ratings, and the detent/shaft mechanical drawings are unconfirmed. The PEC09
  datasheet returned HTTP 403 and needs another source.
- **Riverdi RVT50HQBNWN00**: the 20-pin FPC pinout and the **1.2 A backlight current** are both
  unverified — and that current figure is what sizes the dash 3.3 V buck.

---

## 6. What this episode changed

Three of the four defects found so far were in the *same* document (the wheel pin map), and all
three came from writing plausible-looking detail from memory instead of reading a table. The pin map
was additionally labelled "single source of truth" and "freeze it", which would have propagated the
errors into firmware, the harness drawing, and the board order simultaneously.

New standing rule (added to `engineering-rigor.md` as rule 1a): **no pin number, pin name, package
pin count, or component value enters a document until it has been read out of the datasheet.** Where
a value is genuinely a placeholder, it must say so in the text — "spec, select at capture" — rather
than appearing as a decision.
