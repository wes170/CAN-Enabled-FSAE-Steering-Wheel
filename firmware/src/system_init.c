/* system_init.c — bring the MCU up to 170 MHz and clear the two silicon traps
 * that will otherwise make this board fail in confusing ways.
 *
 * Order matters here, and not for style reasons:
 *
 *   1. ucpd_dead_battery_disable()   MUST run before any GPIO is configured.
 *   2. clock_init()                  PLL, flash latency, boost mode.
 *   3. boot_guard_check()            reports a mis-provisioned board loudly.
 *
 * Both (1) and (3) exist because of defects found in review, and both concern
 * hardware behaviour that happens *before* your code has an opinion. See
 * memory/datasheet-verification.md defects 1.3, 1.6 and 8.2.
 */

#include "board_config.h"
#include "clock_config.h"
#include "system_init.h"
#include <stdbool.h>
#include <stdint.h>

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
/* Native/host build for logic testing. The register writes below are compiled
 * out; everything that is pure arithmetic still gets checked. */
#  define FIRMWARE_HOST_BUILD 1
#endif

/* ---------------------------------------------------------------------------
 * 1. UCPD dead-battery pull-downs  (defect 8.2)
 *
 * DS12288 Table 12 note 6: a 5.1 kohm pull-down is switched onto PB6
 * (UCPD1_CC1) whenever PA9 (UCPD1_DBCC1) is high, and onto PB4 (UCPD1_CC2)
 * whenever PA10 (UCPD1_DBCC2) is high. This happens in silicon, with the UCPD
 * peripheral disabled, driven purely by pin voltage.
 *
 * PA9 is DBG_TX. An idle UART line sits HIGH. So the moment the debug UART is
 * enabled, the MCU pulls down its own PB6 -- which is ENC3_A on the wheel
 * (10 kohm pull-up -> 1.11 V, under the 2.31 V V_IH, so encoder 3 dies) and
 * EVE_INT on the dash (47 kohm pull-up -> 0.32 V, interrupt stuck asserted).
 *
 * The fault therefore appears ONLY when a debug cable is attached: it breaks
 * exactly when you go looking for it. There is no hardware fix -- every
 * encoder-capable timer pair on LQFP-64 is allocated, so ENC3 cannot move.
 *
 * Call this FIRST. Before GPIO clocks, before pin configuration, before
 * anything touches PA9/PA10.
 * ------------------------------------------------------------------------ */
void ucpd_dead_battery_disable(void)
{
#ifndef FIRMWARE_HOST_BUILD
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;   /* PWR is clocked off at reset */
    (void)RCC->APB1ENR1;                   /* read back: the write is posted */
    PWR->CR3 |= PWR_CR3_UCPD1_DBDIS;
#endif
}

/* ---------------------------------------------------------------------------
 * 2. Clock tree — HSE 16 MHz -> PLL -> 170 MHz
 *
 * Every frequency and every PLL divider is validated at COMPILE time in
 * clock_config.h against DS12288 Table 46. If those parameters are ever edited
 * into an illegal combination the build fails; nothing here re-checks them.
 * ------------------------------------------------------------------------ */

/* ⚠ UNVERIFIED — RM0440 has not been readable (ST's server has failed three
 * download attempts). Two things in this function come from prior knowledge
 * rather than from a datasheet read, which violates standing rule 1a, so they
 * are marked rather than presented as fact:
 *
 *   (a) FLASH_LATENCY_170MHZ = 4 wait states. DS12288 explicitly defers this
 *       to RM0440's "number of wait states according to fHCLK" table.
 *   (b) The exact Range-1-Boost entry sequence, in particular whether the AHB
 *       prescaler must be held at /2 across the transition.
 *
 * Both are BENCH-VERIFIABLE at bring-up and both fail loudly, not silently:
 * too few wait states gives immediate hard faults on flash reads, and a bad
 * boost sequence gives a core that never reaches 170 MHz. Confirm both against
 * RM0440 before this leaves the bench; until then treat step 3 of the staged
 * bring-up as the check. */
#define FLASH_LATENCY_170MHZ 4u   /* UNVERIFIED - see note above */

bool clock_init(void)
{
#ifdef FIRMWARE_HOST_BUILD
    return true;
#else
    /* --- HSE on. The crystal is mandatory: HSI16 is +-1% over temperature and
     * 1 Mbit CAN needs about +-0.5% per node (defect 1.4). --- */
    RCC->CR |= RCC_CR_HSEON;
    for (uint32_t t = 0; !(RCC->CR & RCC_CR_HSERDY); ++t) {
        if (t > 100000u) return false;      /* dead or missing crystal */
    }

    /* --- Voltage scaling before speed, never after. --- */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | PWR_CR1_VOS_0;   /* Range 1 */
    while (PWR->SR2 & PWR_SR2_VOSF) { }

#if CORE_NEEDS_BOOST_MODE
    /* DS12288: Range 1 *Boost* is required for 150 MHz < fHCLK <= 170 MHz.
     * R1MODE = 0 selects boost. */
    PWR->CR5 &= ~PWR_CR5_R1MODE;
#else
    PWR->CR5 |= PWR_CR5_R1MODE;
#endif

    /* --- Flash wait states BEFORE raising the frequency. Raising the clock
     * first would execute from flash that cannot keep up. --- */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY)
               | FLASH_LATENCY_170MHZ
               | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_LATENCY_170MHZ) { }

    /* --- PLL. Must be OFF while its configuration register is written. --- */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }

    /* Register encodings differ per field and that asymmetry is a trap:
     *   PLLM field holds (M - 1)
     *   PLLN field holds N directly
     *   PLLR field holds 2/4/6/8 as 0/1/2/3, i.e. (R/2 - 1) */
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE
                 | ((uint32_t)(PLL_M - 1u) << RCC_PLLCFGR_PLLM_Pos)
                 | ((uint32_t)(PLL_N)      << RCC_PLLCFGR_PLLN_Pos)
                 | ((uint32_t)(PLL_R / 2u - 1u) << RCC_PLLCFGR_PLLR_Pos)
                 | RCC_PLLCFGR_PLLREN;

    RCC->CR |= RCC_CR_PLLON;
    for (uint32_t t = 0; !(RCC->CR & RCC_CR_PLLRDY); ++t) {
        if (t > 100000u) return false;      /* never locked (tLOCK max 40 us) */
    }

    /* --- Bus prescalers before the switch, so no bus is briefly overclocked. */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2));

    /* --- Switch SYSCLK to the PLL. --- */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    /* --- FDCAN kernel clock. This is a MUX and its reset value is not PCLK1;
     * leaving it alone gives a node running at the wrong bit rate that looks
     * nearly correct on a scope. clock_config.h assumes PCLK1. --- */
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_FDCANSEL) | RCC_CCIPR_FDCANSEL_1;

    SystemCoreClock = HCLK_HZ;
    return true;
#endif
}

/* ---------------------------------------------------------------------------
 * 3. Boot guard  (defects 1.3 and 1.6)
 *
 * PB8 is both FDCAN1_RX and PB8-BOOT0. An idle CAN bus is recessive = HIGH, so
 * if BOOT0 is taken from the pin the MCU jumps to the system bootloader at
 * every power-on with a live bus -- while booting perfectly on the bench with
 * the bus unplugged.
 *
 * The fix is option bytes, set with STM32CubeProgrammer:
 *     nSWBOOT0 (FLASH_OPTR bit 26) = 0   BOOT0 comes from the option bit
 *     nBOOT0   (FLASH_OPTR bit 27) = 1   boot from main flash
 *
 * Boot mode latches during reset, so this CANNOT be repaired in firmware. All
 * this function can do is notice and complain -- which is the entire point: a
 * mis-provisioned board should announce itself rather than mysteriously
 * refusing to run in the car. Re-check after any mass erase; a full chip erase
 * restores nSWBOOT0 = 1 and silently re-arms the fault.
 *
 * (An earlier revision of these docs called this bit "nBOOT_SEL". That name
 *  belongs to other STM32 families, not G4 -- defect 1.6.)
 * ------------------------------------------------------------------------ */
#define OPTR_NSWBOOT0_Msk (1u << 26)
#define OPTR_NBOOT0_Msk   (1u << 27)

bool boot_guard_check(void)
{
#ifdef FIRMWARE_HOST_BUILD
    return true;
#else
    const uint32_t optr = FLASH->OPTR;
    const bool nswboot0 = (optr & OPTR_NSWBOOT0_Msk) != 0u;
    const bool nboot0   = (optr & OPTR_NBOOT0_Msk)   != 0u;
    /* Correct provisioning is nSWBOOT0 == 0 and nBOOT0 == 1. */
    return (!nswboot0) && nboot0;
#endif
}

/* Pure predicate, separated out so it can be unit-tested on the host without a
 * chip present. Given a raw FLASH_OPTR value, is this board provisioned safely? */
bool boot_option_bytes_ok(uint32_t optr)
{
    return ((optr & OPTR_NSWBOOT0_Msk) == 0u) && ((optr & OPTR_NBOOT0_Msk) != 0u);
}

/* ---------------------------------------------------------------------------
 * Entry point for the startup sequence. Keep this order.
 * ------------------------------------------------------------------------ */
system_init_result_t system_init(void)
{
    system_init_result_t r = { 0 };

    ucpd_dead_battery_disable();      /* before ANY GPIO touches PA9/PA10 */
    r.clock_ok      = clock_init();
    r.boot_bytes_ok = boot_guard_check();
    r.sysclk_hz     = HCLK_HZ;
    return r;
}
