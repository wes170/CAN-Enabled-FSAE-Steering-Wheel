/*
 * board_config.h — single source of truth for pin assignment, both boards.
 *
 * Derived from the VERIFIED pin maps:
 *   wheel : memory/wheel-schematic-complete.md  §9
 *   dash  : memory/dash-schematic-complete.md   §8
 * Evidence for every assignment: memory/datasheet-verification.md §1 and §6.
 *
 * RULE: if this file and the schematic definition ever disagree, the schematic
 * definition is right and this file is a bug. Do not "fix" a mismatch here.
 *
 * Select the board with exactly one of:  -DBOARD_WHEEL   or   -DBOARD_DASH
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#if defined(BOARD_WHEEL) == defined(BOARD_DASH)
#error "Define exactly one of BOARD_WHEEL or BOARD_DASH"
#endif

/* ---------------------------------------------------------------------------
 * Shared by both boards
 * ------------------------------------------------------------------------ */

/* FDCAN1. On LQFP-64 the only options are PA11/PA12 or PB8/PB9, and USB takes
 * PA11/PA12 — so CAN is forced onto PB8/PB9. PB8 is also BOOT0. See below. */
#define CAN_RX_PORT   GPIOB
#define CAN_RX_PIN    8u
#define CAN_TX_PORT   GPIOB
#define CAN_TX_PIN    9u
#define CAN_BITRATE   1000000u          /* Haltech bus, 1 Mbit/s */

/*  ⚠ BOOT0 / PB8 HAZARD — read before first flash.
 *
 *  PB8 is PB8-BOOT0 as well as FDCAN1_RX. An idle CAN bus is *recessive*,
 *  which is logic HIGH. If BOOT0 is taken from the pin, the MCU samples
 *  BOOT0 = 1 at every power-on with a live bus and jumps into the system
 *  bootloader — while booting perfectly on the bench with the bus unplugged.
 *
 *  Mitigation is an OPTION-BYTE setting, not code. On STM32G4 the two bits are:
 *      nSWBOOT0 (FLASH_OPTR[26]) = 0   -> BOOT0 comes from the option bit,
 *                                         NOT from the PB8 pin
 *      nBOOT0   (FLASH_OPTR[27]) = 1   -> boot from main flash
 *
 *  Set them with STM32CubeProgrammer. Boot mode is latched during the reset
 *  sequence, so firmware CANNOT fix this after the fact — there is no code
 *  workaround for a mis-provisioned board.
 *
 *  Verify on every board, and RE-CHECK AFTER ANY MASS ERASE: a full chip
 *  erase can restore the factory option-byte state (nSWBOOT0 = 1), which
 *  silently re-arms the failure.
 *
 *  boot_guard_check() in system_init.c reads these bits back at startup and
 *  complains loudly, so a mis-provisioned board announces itself instead of
 *  mysteriously refusing to run in the car.
 *
 *  (An earlier revision of these docs said "nBOOT_SEL = 1" — that bit name
 *   belongs to other STM32 families, not G4. Defect 1.6.)
 */

/*  ⚠ UCPD DEAD-BATTERY HAZARD — PA9/PA10 silently sabotage PB6/PB4.
 *
 *  Neither board uses USB Type-C Power Delivery. It does not matter: the
 *  dead-battery pull-downs are armed by PIN VOLTAGE, not by enabling UCPD.
 *
 *  DS12288 Table 12 note 6: a 5.1 kohm pull-down (Rd) is activated on
 *      PB6 (UCPD1_CC1) by a high level on PA9  (UCPD1_DBCC1)
 *      PB4 (UCPD1_CC2) by a high level on PA10 (UCPD1_DBCC2)
 *
 *  PA9 is DBG_TX and an idle UART line sits HIGH, so initialising the debug
 *  UART arms Rd on PB6 -- which is ENC3_A on the wheel. Against its 10 kohm
 *  conditioning pull-up that gives 3.3 * 5.1/15.1 = 1.11 V, under the 2.31 V
 *  FT_c V_IH, so ENC3_A never reads high and TIM4 decode dies. On the dash
 *  PB4 is EVE_INT (47 kohm internal pull-up) -> 0.32 V, i.e. the active-low
 *  display interrupt is stuck asserted whenever a debug adapter is attached.
 *
 *  So the bug appears only while you are watching it. Clear it FIRST, before
 *  any GPIO configuration, in system_init():
 *
 *      RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
 *      PWR->CR3      |= PWR_CR3_UCPD1_DBDIS;
 *
 *  No hardware fix exists: every encoder-capable timer pair on LQFP-64 is
 *  already allocated, so ENC3 cannot move off PB6. Defect 8.2.
 */
#define UCPD_DEAD_BATTERY_MUST_BE_DISABLED  1

/* HSE crystal — mandatory. HSI16 is -1%/+1% over 0..85 C; CAN needs roughly
 * ±0.5% per node and that budget is shared with every other node. */
#define HSE_FREQ_HZ   16000000u         /* confirm against the fitted crystal */
#define SYSCLK_HZ     170000000u

/*  ADC reference and range — a property of the BOARD, not of any one driver,
 *  so it lives here once. Both boards run VREF+ from the filtered +3V3A rail
 *  through FB1, and both use the G474's 12-bit ADCs.
 *
 *  ⚠ This used to be defined twice: daq.h (dash) and power.h (wheel). The two
 *  copies never collided, because those headers sit in mutually exclusive
 *  board branches -- which is exactly what makes that kind of duplication
 *  survive. Same shape as defect 8.1: one fact, two homes, and nothing forcing
 *  them to agree.
 *
 *  The 3.3 V figure is the NOMINAL rail. Regulator initial accuracy is
 *  typically ±2%, so every reading inherits that as a gain error; for the
 *  temperatures, pressures and rail monitors this feeds, 2% is acceptable. If
 *  a channel ever needs better, the fix is ratiometric sensing (excite the
 *  sensor from the same rail and the error cancels), not a bigger number. */
#define ADC_VREF_MV    3300u
#define ADC_FULL_SCALE 4095u

/* Debug UART (USART1) and SWD are identical on both boards. */
#define DBG_UART_TX_PORT  GPIOA
#define DBG_UART_TX_PIN   9u
#define DBG_UART_RX_PORT  GPIOA
#define DBG_UART_RX_PIN   10u

/* ---------------------------------------------------------------------------
 * WHEEL
 * ------------------------------------------------------------------------ */
#ifdef BOARD_WHEEL

#define BOARD_NAME "FSAE-WHEEL"

/*  Encoders. Hardware quadrature decoding requires BOTH:
 *    (1) the two pins are CH1 and CH2 of the SAME timer, and
 *    (2) that timer actually implements encoder mode.
 *
 *  On STM32G474 the timers with an encoder interface are
 *    TIM1, TIM2, TIM3, TIM4, TIM5, TIM8, TIM20  (+ LPTIM1).
 *  TIM15/16/17 have channels but NO quadrature decoder — an early revision
 *  of this design put ENC5 on TIM15 and it would not have worked
 *  (defect 1.5). Do not reassign these pins without re-checking both rules.
 */
#define ENC1_TIM      TIM1              /* PC0 = TIM1_CH1,  PC1 = TIM1_CH2  */
#define ENC2_TIM      TIM3              /* PC6 = TIM3_CH1,  PC7 = TIM3_CH2  */
#define ENC3_TIM      TIM4              /* PB6 = TIM4_CH1,  PB7 = TIM4_CH2  */
#define ENC4_TIM      TIM2              /* PA15 = TIM2_CH1, PB3 = TIM2_CH2  */
#define ENC5_TIM      TIM20             /* PB2 = TIM20_CH1, PC2 = TIM20_CH2 */
#define ENC6_TIM      TIM5              /* PA0 = TIM5_CH1,  PC12 = TIM5_CH2 */
#define ENC_COUNT     6u

/* Detents per revolution — sets the AVI scaling in the IO12 emulation. */
#define ENC_DETENTS_THUMB      12u      /* PEC09-2120F-S0012 */
#define ENC_DETENTS_FACEPLATE  20u      /* PEC11H-4120F-S0020 */

/* Encoder push switches, then buttons. All active-low with external 10k
 * pull-ups to +3V3 and a 1k/100nF RC at the pin (~100us fall, ~1ms rise). */
#define ENC_SW_PINS   { {GPIOC,4}, {GPIOC,5}, {GPIOC,8}, \
                        {GPIOC,9}, {GPIOC,10}, {GPIOC,11} }
#define BTN_PINS      { {GPIOC,13}, {GPIOD,2}, {GPIOA,3}, \
                        {GPIOB,10}, {GPIOB,11}, {GPIOB,12} }
#define BTN_COUNT     6u

/* LEDs: 16 shift + 4 TC + 4 lockup, one WS2812B-2020 chain via a
 * 74AHCT1G125 level shifter (3.3V logic cannot meet the LED's 0.7*VDD). */
#define LED_DATA_PORT GPIOA
#define LED_DATA_PIN  6u                /* TIM16_CH1 + DMA */
#define LED_DATA_TIM  TIM16
#define LED_SHIFT_COUNT   16u
#define LED_TC_COUNT       4u
#define LED_LOCKUP_COUNT   4u
#define LED_TOTAL     (LED_SHIFT_COUNT + LED_TC_COUNT + LED_LOCKUP_COUNT)

/*  Global LED current cap. 24 LEDs at full white is ~0.86 A, which the 12 V
 *  input stage can supply but the thermal and connector budget should not
 *  see routinely. Every pattern goes through led_apply_cap(); nothing writes
 *  the strip directly.
 *
 *  These are the two CEILINGS, not a choice made here. Rev B.5 fitted `R_VBUS`
 *  on every board, so one binary on one board can be vehicle-powered or
 *  bus-powered at different moments -- see power.h, which picks between them at
 *  run time from a V12_SENSE reading. There is no -DBOARD_SIM any more.
 *
 *  The USB figure was 350 mA while the CAN transceiver was DNP in the SIM
 *  variant. Fitting it on every board spends 70 mA the old number never saw:
 *      500 mA allowance - 110 mA (3V3 via LDO) - 70 mA (TJA1051 dominant) = 320
 *  so the cap drops to 300 mA, keeping 20 mA in hand. Collapsing the variants
 *  changed this number; see power.h for the full derivation. */
#define LED_CURRENT_CAP_MA_CAR  450u
#define LED_CURRENT_CAP_MA_USB  300u

/* Display: Sharp LS027B7DH01, 400x240 mono, 2.7in reflective memory-in-pixel.
 * Rev B.11 replaced the JDI LPM013M126A (176x176, 8 colours) with this panel.
 * The 10-pin FPC pinout is IDENTICAL, so none of the pins below moved -- but
 * the panel now runs from +5V and the wire format is a different protocol.
 * ⚠ NOT pin-compatible with the LS013B7DH05 in any useful sense: that part is
 * 3 V and this board's display rail is now 5 V (defect 8.24). */
#define LCD_SCLK_PORT     GPIOA
#define LCD_SCLK_PIN      5u            /* SPI1_SCK  */
#define LCD_SI_PORT       GPIOA
#define LCD_SI_PIN        7u            /* SPI1_MOSI */
#define LCD_SCS_PORT      GPIOA
#define LCD_SCS_PIN       4u            /* GPIO      */
#define LCD_DISP_PORT     GPIOB
#define LCD_DISP_PIN      13u           /* moved off PC2 to free TIM20_CH2  */
#define LCD_EXTCOMIN_PORT GPIOC
#define LCD_EXTCOMIN_PIN  3u

/*  ⚠ Three display constraints that are invisible on the schematic:
 *   1. SCLK maximum is 2.00 MHz (1.00 MHz typical). The STM32 will happily
 *      clock SPI1 at 20 MHz+. Cap it or the panel misbehaves.
 *   2. EXTCOMIN must be toggled continuously at ~1 Hz whenever the panel is
 *      powered. It is what prevents a DC bias building across the liquid
 *      crystal. Stop toggling and you eventually damage the display.
 *   3. NEW with this panel: the frame must be REWRITTEN PERIODICALLY even when
 *      nothing changes. A still image must not stand more than two hours, and
 *      a static-discharge event can drop pixel memory outright -- on a wheel
 *      gripped by a driver in a synthetic-fibre suit, that is not
 *      hypothetical. See LCD_ANTI_STICK_REFRESH_MS in display_wheel.h.  */
#define LCD_SPI_MAX_HZ    2000000u
#define LCD_EXTCOMIN_HZ   1u

/* Paddle sense taps — observation only. The paddle signals themselves are
 * copper from the connector to the ECU and do not pass through this MCU.
 * When the wheel is USB-powered (sim rig) these become the shift inputs --
 * a run-time personality now, not a build variant. See power.h.
 *
 * READ THESE WITH THE ADC, NOT AS GPIO. PB0/PB1 are TT_a pins (4.0 V absolute
 * max input) and the Nexus pulls the paddle lines to 5 V or 12 V depending on
 * configuration. The 150k/39k divider puts 16 V at 3.30 V and 5 V at 1.03 V --
 * both safe, but 1.03 V is below the 2.31 V logic-high threshold, so no single
 * divider serves both pull-ups as a digital input. Threshold in firmware.
 * (Defect 1.8.) */
#define PADDLE_UP_SNS_PORT  GPIOB
#define PADDLE_UP_SNS_PIN   0u          /* ADC1_IN15 */
#define PADDLE_DN_SNS_PORT  GPIOB
#define PADDLE_DN_SNS_PIN   1u          /* ADC1_IN12 */
#define PADDLE_DIVIDER_NUM  189u        /* (150k + 39k) / 39k -> Vline = Vpin * 189/39 */
#define PADDLE_DIVIDER_DEN  39u
#define PADDLE_THRESH_MV    500u        /* above this = line high (pulled up) */

/* Rail monitors.
 *
 * CHANNEL NUMBERS, NOT PIN NUMBERS. PA0 is IN1, so PA0..PA3 are IN1..IN4 --
 * the index is offset by one from the pin number and it is easy to land one
 * either side and have both feel right. Verified in DS12288 Table 12.
 * These were previously 3 and 4 (defect 8.3): the 12 V monitor actually
 * sampled PA2 and read a plausible-looking 14.25 V, and the 5 V monitor
 * sampled PA3 = BTN3, so pressing button 3 faked a 5 V rail collapse. */
#define V12_SENSE_CH  2u                /* PA1 = ADC12_IN2, 47k/10k divider */
#define V5_SENSE_CH   3u                /* PA2 = ADC1_IN3,  10k/10k divider */

#endif /* BOARD_WHEEL */

/* ---------------------------------------------------------------------------
 * DASH
 * ------------------------------------------------------------------------ */
#ifdef BOARD_DASH

#define BOARD_NAME "FSAE-DASH"

/* Display: Riverdi RVT50HQBNWN00, BT817 EVE coprocessor over SPI.
 * NOTE the clock ceiling here is 30 MHz — the BT817's limit. Do not confuse
 * it with the wheel panel's 2 MHz. Different part, different board. */
#define EVE_CS_PORT    GPIOA
#define EVE_CS_PIN     4u
#define EVE_SCK_PORT   GPIOA
#define EVE_SCK_PIN    5u
#define EVE_MISO_PORT  GPIOA
#define EVE_MISO_PIN   6u
#define EVE_MOSI_PORT  GPIOA
#define EVE_MOSI_PIN   7u
#define EVE_PDN_PORT   GPIOB
#define EVE_PDN_PIN    3u               /* active low, internal 47k pull-up */
#define EVE_INT_PORT   GPIOB
#define EVE_INT_PIN    4u               /* active low, internal 47k pull-up */
#define EVE_SPI_MAX_HZ 30000000u

/* DAQ: 8 channels, all reachable by ADC1/ADC2 so one scan sequence covers
 * them. Divider is /2, so 0-5 V in maps to 0-2.5 V at the pin. */
#define AIN_PINS  { {GPIOA,0}, {GPIOA,1}, {GPIOA,2}, {GPIOA,3}, \
                    {GPIOC,0}, {GPIOC,1}, {GPIOC,2}, {GPIOC,3} }

/* ADC CHANNEL numbers for the pins above, in the same order. Do NOT derive
 * these from the pin numbers -- PA0 is IN1 and PC0 is IN6, so neither port
 * maps one-to-one. Read from DS12288 Table 12; getting this wrong on the
 * wheel rail monitors was defect 8.3. */
#define AIN_CHANNELS  { 1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u }
/*                     PA0 PA1 PA2 PA3 PC0 PC1 PC2 PC3
 * PA0 ADC12_IN1 · PA1 ADC12_IN2 · PA2 ADC1_IN3  · PA3 ADC1_IN4
 * PC0 ADC12_IN6 · PC1 ADC12_IN7 · PC2 ADC12_IN8 · PC3 ADC12_IN9
 * All eight reach ADC1, so a single scan sequence covers the set. */
#define AIN_COUNT        8u
#define AIN_DIVIDER_NUM  2u             /* Vin = Vadc * 2 */
#define AIN_ARB_FB_FRONT 6u             /* index of AIN7 */
#define AIN_ARB_FB_REAR  7u             /* index of AIN8 */

/*  ADC sample time. The front end presents a 5 kohm Thevenin source, and a
 *  sample time too short for that impedance gives a reading that has not
 *  settled -- an error that scales with source impedance and looks exactly
 *  like a miscalibrated sensor.
 *
 *  RESOLVED BY CHOOSING THE EXTREME rather than by looking up the table.
 *  The correct minimum lives in an RM0440 source-impedance table that has not
 *  been readable (ST's server has failed four download attempts). Instead of
 *  carrying an unverified number, use the LONGEST sample time the hardware
 *  offers: SMP = 111 = 640.5 cycles. If that is not enough for 5 kohm then no
 *  setting is, and the DNP TLV9004 buffer has to be fitted -- so this choice
 *  cannot be wrong in the dangerous direction.
 *
 *  It costs nothing. DS12288 gives fADC(max) = 52 MHz (Range 1, all ADCs,
 *  single-ended, VDDA >= 2.7 V) and fs = fADC / (t_s + resolution + 0.5), so
 *  one channel is (640.5 + 12 + 0.5) / 52 MHz = 12.56 us and a full 8-channel
 *  scan is 100 us. At a 100 Hz DAQ rate that is 1% of the time budget.
 *
 *  Trading 1% of a timer for an open datasheet question is a good trade; the
 *  earlier value here (92.5 cycles) was written from memory and marked
 *  UNVERIFIED, which is a liability that never expires on its own. */
#define AIN_SMP_REGVAL     7u      /* SMP = 111 */
#define AIN_SAMPLE_CYCLES  640u    /* 640.5, rounded down for arithmetic */
#define ADC_MAX_CLOCK_HZ   52000000u

/* ARB servo outputs — TIM4 CH1/CH2, so both share a timebase. */
#define SERVO_TIM        TIM4
#define SERVO1_PORT      GPIOB
#define SERVO1_PIN       6u             /* TIM4_CH1 */
#define SERVO2_PORT      GPIOB
#define SERVO2_PIN       7u             /* TIM4_CH2 */
#define SERVO_FRAME_HZ   50u
#define SERVO_PULSE_MIN_US 1000u        /* firmware end-stop, per channel   */
#define SERVO_PULSE_MAX_US 2000u        /* narrow these to the real travel! */
#define SERVO_SLEW_US_PER_S 500u        /* full travel no faster than ~2 s  */
#define SERVO_CAN_TIMEOUT_MS 500u
#define SERVO_DIVERGENCE_TIMEOUT_MS 500u

/*  ⚠ ARB SAFETY — these four are requirements, not tuning parameters.
 *   1. On CAN loss, HOLD the last commanded position. Never spring to centre;
 *      an ARB step change mid-corner is a handling event.
 *   2. End-stops (SERVO_PULSE_MIN/MAX_US) must be narrowed to the real
 *      mechanical travel and proven on the bench BEFORE a servo is bolted to
 *      a linkage. A servo driven into a hard stop draws stall current until
 *      it burns.
 *   3. Slew limiting so a knob spin cannot slam the linkage.
 *   4. Divergence alarm: commanded vs measured beyond threshold for longer
 *      than SERVO_DIVERGENCE_TIMEOUT_MS means a seized linkage, stripped
 *      spline or dead servo. Flag it on the dash and log it.
 */

/* Local buttons and rail monitors */
#define BTN_PINS  { {GPIOC,8}, {GPIOC,9}, {GPIOC,10} }
#define BTN_COUNT 3u
#define V12_SENSE_PORT GPIOB
#define V12_SENSE_PIN  0u
#define V5_SENSE_PORT  GPIOB
#define V5_SENSE_PIN   1u

#endif /* BOARD_DASH */

#endif /* BOARD_CONFIG_H */
