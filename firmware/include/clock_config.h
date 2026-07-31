/* clock_config.h — PLL parameters, with the datasheet's limits enforced by the
 * compiler rather than stated in a comment.
 *
 * Why this file exists separately from board_config.h: defect 8.3. A number that
 * is merely *written down correctly* still gets edited later by someone who does
 * not know which constraint it was satisfying. Every limit below is a real line
 * from STM32G474 datasheet DS12288, and every one is checked by a static
 * assertion, so an incompatible edit fails the build instead of the board.
 *
 * All limits: DS12288 Rev 6, Table 46 "PLL characteristics", plus §"operating
 * conditions" for the voltage-scaling ranges.
 */
#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#include "board_config.h"

/* ---------------------------------------------------------------------------
 * Datasheet limits — DO NOT EDIT to make a build pass. If one of the assertions
 * below fires, the PLL parameters are wrong, not these numbers.
 * ------------------------------------------------------------------------ */
#define DS_PLL_IN_MIN_HZ        2660000u    /* Table 46 fPLL_IN  min 2.66 MHz  */
#define DS_PLL_IN_MAX_HZ       16000000u    /* Table 46 fPLL_IN  max 16 MHz    */
#define DS_VCO_MIN_HZ          96000000u    /* Table 46 fVCO_OUT min 96 MHz    */
#define DS_VCO_MAX_HZ         344000000u    /* Table 46 fVCO_OUT max 344 MHz   */
#define DS_PLLR_MIN_HZ          8000000u    /* Table 46 fPLL_R_OUT min 8 MHz   */
#define DS_PLLR_MAX_BOOST_HZ  170000000u    /* Table 46, Range 1 BOOST mode    */
#define DS_PLLR_MAX_RANGE1_HZ 150000000u    /* Table 46, Range 1 normal mode   */

/* Above 150 MHz the core must run in Range 1 *Boost* mode. DS12288:
 *   "Voltage Range 1 Boost mode for 150 MHz < fHCLK <= 170 MHz
 *    Voltage Range 1 Normal mode for 26 MHz < fHCLK <= 150 MHz"
 * This is not an optimisation -- at 170 MHz in normal mode the core is
 * under-volted and fails in ways that look like random corruption. */
#define DS_BOOST_REQUIRED_ABOVE_HZ 150000000u

/* PLL lock time, Table 46: 15 us typ / 40 us max. The startup code waits on the
 * ready flag rather than on time, but a timeout has to be longer than this. */
#define DS_PLL_LOCK_MAX_US 40u

/* ---------------------------------------------------------------------------
 * Chosen configuration: HSE 16 MHz -> 170 MHz SYSCLK
 *
 *   PLL input = HSE / M      = 16 / 4  =   4 MHz
 *   VCO       = PLL input * N=  4 * 85 = 340 MHz
 *   SYSCLK    = VCO / R      = 340 / 2 = 170 MHz
 *
 * M and R are the real divisors here, not the register encodings. The register
 * fields are computed at the point of use, because PLLM and PLLR encode
 * differently (PLLM stores M-1; PLLR stores 2/4/6/8 as 0/1/2/3) and that
 * asymmetry is exactly the kind of thing that produces a board which boots at
 * the wrong speed and fails only on CAN bit timing.
 * ------------------------------------------------------------------------ */
#define PLL_M  4u
#define PLL_N  85u
#define PLL_R  2u

#define PLL_IN_HZ   (HSE_FREQ_HZ / PLL_M)
#define PLL_VCO_HZ  (PLL_IN_HZ * PLL_N)
#define PLL_R_HZ    (PLL_VCO_HZ / PLL_R)

/* AHB/APB prescalers. Both APB buses run at full 170 MHz. */
#define AHB_PRESCALER  1u
#define APB1_PRESCALER 1u
#define APB2_PRESCALER 1u

#define HCLK_HZ  (PLL_R_HZ / AHB_PRESCALER)
#define PCLK1_HZ (HCLK_HZ / APB1_PRESCALER)
#define PCLK2_HZ (HCLK_HZ / APB2_PRESCALER)

/* ---------------------------------------------------------------------------
 * Compile-time verification against the datasheet
 * ------------------------------------------------------------------------ */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define CLK_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#  define CLK_ASSERT(cond, msg) typedef char clk_assert_##__LINE__[(cond) ? 1 : -1]
#endif

CLK_ASSERT(PLL_IN_HZ >= DS_PLL_IN_MIN_HZ,
           "PLL input below DS12288 Table 46 minimum of 2.66 MHz - increase PLL_M's divisor budget");
CLK_ASSERT(PLL_IN_HZ <= DS_PLL_IN_MAX_HZ,
           "PLL input above DS12288 Table 46 maximum of 16 MHz - PLL_M is too small");
CLK_ASSERT(PLL_VCO_HZ >= DS_VCO_MIN_HZ,
           "PLL VCO below DS12288 Table 46 minimum of 96 MHz");
CLK_ASSERT(PLL_VCO_HZ <= DS_VCO_MAX_HZ,
           "PLL VCO above DS12288 Table 46 maximum of 344 MHz - PLL_N is too large");
CLK_ASSERT(PLL_R_HZ >= DS_PLLR_MIN_HZ,
           "PLL R output below DS12288 Table 46 minimum of 8 MHz");
CLK_ASSERT(PLL_R_HZ <= DS_PLLR_MAX_BOOST_HZ,
           "PLL R output above 170 MHz - beyond the part's rating in any voltage range");
CLK_ASSERT(PLL_R_HZ == SYSCLK_HZ,
           "PLL output does not equal SYSCLK_HZ declared in board_config.h - one of them is a lie");

/* R must be one of the four encodable values. */
CLK_ASSERT(PLL_R == 2u || PLL_R == 4u || PLL_R == 6u || PLL_R == 8u,
           "PLLR can only divide by 2, 4, 6 or 8");
/* M is a 4-bit field holding M-1, so 1..16. */
CLK_ASSERT(PLL_M >= 1u && PLL_M <= 16u, "PLLM must be 1..16");
/* N is 8..127. */
CLK_ASSERT(PLL_N >= 8u && PLL_N <= 127u, "PLLN must be 8..127");

/* Integer division must be exact, or the frequencies above are fiction and every
 * CAN bit-timing calculation derived from them is off. */
CLK_ASSERT(HSE_FREQ_HZ % PLL_M == 0u, "HSE does not divide evenly by PLL_M");
CLK_ASSERT(PLL_VCO_HZ % PLL_R == 0u, "VCO does not divide evenly by PLL_R");

/* Does this configuration require Boost mode? Answer it here, once, rather than
 * leaving it to be remembered in the startup code. */
#if (PLL_R_HZ > DS_BOOST_REQUIRED_ABOVE_HZ)
#  define CORE_NEEDS_BOOST_MODE 1
CLK_ASSERT(PLL_R_HZ <= DS_PLLR_MAX_BOOST_HZ, "above 170 MHz even Boost mode is not enough");
#else
#  define CORE_NEEDS_BOOST_MODE 0
CLK_ASSERT(PLL_R_HZ <= DS_PLLR_MAX_RANGE1_HZ, "Range 1 normal mode tops out at 150 MHz");
#endif

/* ---------------------------------------------------------------------------
 * CAN bit timing sanity — FDCAN is clocked from PCLK1 here.
 *
 * The whole reason this board carries a crystal (defect 1.4) is that 1 Mbit/s
 * CAN needs roughly +-0.5% clock accuracy and the internal RC oscillator is
 * +-1% over temperature. If the clock tree is wrong, CAN failures look like
 * firmware bugs. Assert that PCLK1 divides evenly into the bit rate so the
 * bit-timing registers can actually be satisfied.
 * ------------------------------------------------------------------------ */
#define CAN_BITRATE_HZ 1000000u

/* FDCAN kernel clock source. On STM32G4 this is a mux (RCC_CCIPR.FDCANSEL:
 * HSE / PLL"Q" / PCLK1) -- it is NOT automatically PCLK1, and getting this
 * wrong produces a node that transmits at the wrong rate and looks, on a
 * scope, almost right. Set it explicitly in system_init(). */
#define FDCAN_KERNEL_CLK_HZ PCLK1_HZ

CLK_ASSERT(FDCAN_KERNEL_CLK_HZ % CAN_BITRATE_HZ == 0u,
           "FDCAN kernel clock is not an integer multiple of 1 Mbit/s - bit timing cannot be exact");

/* Nominal bit timing. One bit = BRP * (1 sync + NTSEG1 + NTSEG2) time quanta.
 *
 *   170 MHz / 1 Mbit/s = 170 tq per bit before the prescaler.
 *   BRP = 5  ->  34 tq per bit, which gives fine enough resolution to place the
 *                sample point accurately without running the register values
 *                near their limits.
 *   sample point = (1 + NTSEG1) / 34 = 27/34 = 79.4%
 *
 * CiA recommends 75-80% at 1 Mbit/s. The sample point is the one bit-timing
 * number worth caring about: every node on the bus must agree closely, and
 * Haltech's own nodes sit near 80%. */
#define CAN_BRP     5u
#define CAN_NTSEG1  26u
#define CAN_NTSEG2  7u
#define CAN_NSJW    7u      /* <= NTSEG2 */

#define CAN_TQ_PER_BIT (1u + CAN_NTSEG1 + CAN_NTSEG2)
/* Sample point in tenths of a percent, integer maths only. */
#define CAN_SAMPLE_POINT_PERMILLE (((1u + CAN_NTSEG1) * 1000u) / CAN_TQ_PER_BIT)

CLK_ASSERT(FDCAN_KERNEL_CLK_HZ == CAN_BRP * CAN_TQ_PER_BIT * CAN_BITRATE_HZ,
           "BRP/NTSEG1/NTSEG2 do not reconstruct exactly 1 Mbit/s from the kernel clock");
CLK_ASSERT(CAN_SAMPLE_POINT_PERMILLE >= 750u && CAN_SAMPLE_POINT_PERMILLE <= 800u,
           "CAN sample point outside the 75-80% CiA recommendation for 1 Mbit/s");
/* FDCAN nominal bit-timing register field ranges. */
CLK_ASSERT(CAN_BRP    >= 1u && CAN_BRP    <= 512u, "NBRP must be 1..512");
CLK_ASSERT(CAN_NTSEG1 >= 1u && CAN_NTSEG1 <= 256u, "NTSEG1 must be 1..256");
CLK_ASSERT(CAN_NTSEG2 >= 1u && CAN_NTSEG2 <= 128u, "NTSEG2 must be 1..128");
CLK_ASSERT(CAN_NSJW <= CAN_NTSEG2, "SJW must not exceed NTSEG2");

/* ---------------------------------------------------------------------------
 * USB 48 MHz — and why it CANNOT come from the crystal (defect 8.17)
 *
 * `wheel-schematic-complete.md` §3.2 used to say: since CAN needs a crystal
 * anyway, "clock both from the HSE and delete a whole class of clock-accuracy
 * questions." The first half is right. The second half is arithmetically
 * impossible on this part, and it went unchallenged because it sounds like
 * exactly the kind of simplification a good design makes.
 *
 * USB FS needs 48 MHz. The only PLL route to it is PLL"Q", and PLLQ divides the
 * same VCO that PLLR divides for SYSCLK:
 *
 *     48 MHz from PLLQ  =>  VCO = 48 * Q,  Q in {2,4,6,8}
 *                       =>  VCO in {96, 192, 288, 384} MHz
 *     170 MHz SYSCLK    =>  VCO = 170 * R, R in {2,4,6,8}
 *                       =>  VCO in {340, 680, 1020, 1360} MHz
 *
 * The two sets do not intersect, and 680 MHz and up are far past the 344 MHz
 * VCO ceiling anyway. There is no PLL configuration that serves 170 MHz SYSCLK
 * and 48 MHz USB at the same time. It is not a tuning problem.
 *
 * So USB runs from **HSI48 + CRS**. CRS (Clock Recovery System) trims HSI48
 * against the host's 1 kHz USB SOF packets, which pulls it far inside USB FS's
 * 2500 ppm requirement without any crystal involvement. The crystal still earns
 * its place — CAN needs it, and that was always the real justification.
 *
 * This matters more since Rev B.5 than it did before: USB used to be a
 * sim-variant feature plus a DFU path, and is now a first-class mode on every
 * board, because `R_VBUS` is fitted everywhere.
 * ------------------------------------------------------------------------ */
#define USB_CLK_HZ 48000000u

/* Guard the reasoning, not just the result. If someone later re-tunes the PLL
 * to a VCO that CAN feed PLLQ at 48 MHz, this fires and asks them to revisit
 * the USB clock source rather than silently leaving HSI48 selected for no
 * reason. An assertion that only ever protects today's numbers is worth less
 * than one that protects the argument. */
CLK_ASSERT(PLL_VCO_HZ !=  96000000u && PLL_VCO_HZ != 192000000u &&
           PLL_VCO_HZ != 288000000u && PLL_VCO_HZ != 384000000u,
           "This VCO can produce 48 MHz on PLLQ - USB no longer has to use HSI48, revisit the choice");

/* CRS reload: the counter measures the target clock between SYNC events, so
 * RELOAD = f_target / f_sync - 1. USB SOF is 1 kHz. This is arithmetic, not a
 * table lookup, which is why it is derived here rather than pasted as 0xBB7F.
 *
 * ⚠ FELIM (the frequency error limit that sets when CRS gives up and declares
 * SYNCERR) is left at its reset value. Its optimum is an RM0440 refinement, and
 * RM0440 has never been downloadable in this project. The reset value works;
 * the tuned value would work better at the margins. Recorded as a refinement,
 * not as a placeholder — nothing here is wrong, it is just not optimal. */
#define CRS_SYNC_HZ    1000u
#define CRS_RELOAD_VAL ((USB_CLK_HZ / CRS_SYNC_HZ) - 1u)

CLK_ASSERT(USB_CLK_HZ % CRS_SYNC_HZ == 0u,
           "USB clock is not an integer multiple of the 1 kHz SOF rate - CRS reload would not be exact");
CLK_ASSERT(CRS_RELOAD_VAL <= 0xFFFFu, "CRS RELOAD is a 16-bit field");
CLK_ASSERT(CRS_RELOAD_VAL == 47999u, "CRS reload should be 47999 for 48 MHz against 1 kHz SOF");

#endif /* CLOCK_CONFIG_H */
