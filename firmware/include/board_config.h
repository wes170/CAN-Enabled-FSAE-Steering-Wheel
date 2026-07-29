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

/* HSE crystal — mandatory. HSI16 is -1%/+1% over 0..85 C; CAN needs roughly
 * ±0.5% per node and that budget is shared with every other node. */
#define HSE_FREQ_HZ   16000000u         /* confirm against the fitted crystal */
#define SYSCLK_HZ     170000000u

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
 *  the strip directly. Sim builds cap harder because USB default is 500 mA. */
#define LED_CURRENT_CAP_MA_CAR  450u
#define LED_CURRENT_CAP_MA_SIM  350u

/* Display: JDI LPM013M126A, 176x176, 8 colours, reflective memory-in-pixel.
 * Pin-compatible with the Sharp LS013B7DH05 mono fallback. */
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

/*  ⚠ Two display constraints that are invisible on the schematic:
 *   1. SCLK maximum is 2.00 MHz (1.00 MHz typical). The STM32 will happily
 *      clock SPI1 at 20 MHz+. Cap it or the panel misbehaves.
 *   2. EXTCOMIN must be toggled continuously at ~1 Hz whenever the panel is
 *      powered. It is what prevents a DC bias building across the liquid
 *      crystal. Stop toggling and you eventually damage the display.  */
#define LCD_SPI_MAX_HZ    2000000u
#define LCD_EXTCOMIN_HZ   1u

/* Paddle sense taps — observation only, 100k series. The paddle signals
 * themselves are copper from the connector to the ECU and do not pass
 * through this MCU. In the SIM build these become the shift inputs. */
#define PADDLE_UP_SNS_PORT  GPIOB
#define PADDLE_UP_SNS_PIN   0u
#define PADDLE_DN_SNS_PORT  GPIOB
#define PADDLE_DN_SNS_PIN   1u

/* Rail monitors */
#define V12_SENSE_CH  3u                /* PA1 = ADC1_IN3, 47k/10k divider */
#define V5_SENSE_CH   4u                /* PA2 = ADC1_IN4, 10k/10k divider */

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
#define AIN_COUNT        8u
#define AIN_DIVIDER_NUM  2u             /* Vin = Vadc * 2 */
#define AIN_ARB_FB_FRONT 6u             /* index of AIN7 */
#define AIN_ARB_FB_REAR  7u             /* index of AIN8 */

/*  ADC sample time must respect the 5k Thevenin source impedance of the
 *  front end. Use a long sample time (>= 92.5 cycles) or the reading will
 *  not settle to 12-bit accuracy.  [UNVERIFIED — this figure came from
 *  memory, not the reference manual. Confirm in RM0440 before trusting it.] */
#define AIN_SAMPLE_CYCLES 92u

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
