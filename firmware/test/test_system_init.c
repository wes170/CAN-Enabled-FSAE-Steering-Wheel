/* test_system_init.c — host-side tests for the parts of startup that are pure
 * logic. Build and run with firmware/test/run-tests.sh
 *
 * There is no hardware here, so this proves exactly two things: the clock
 * arithmetic (via the static assertions in clock_config.h, which fail the
 * BUILD, not the run) and the boot-option-byte predicate. That is the honest
 * limit of what can be checked at a desk -- everything else in system_init.c
 * writes registers and has to wait for a board.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "board_config.h"
#include "clock_config.h"

bool boot_option_bytes_ok(uint32_t optr);   /* from system_init.c */

static int failures = 0;

static void check(const char *what, bool got, bool want)
{
    if (got != want) {
        printf("  FAIL  %-58s got %d want %d\n", what, got, want);
        ++failures;
    } else {
        printf("  ok    %-58s\n", what);
    }
}

static void check_u32(const char *what, uint32_t got, uint32_t want)
{
    if (got != want) {
        printf("  FAIL  %-58s got %u want %u\n", what, got, want);
        ++failures;
    } else {
        printf("  ok    %-58s = %u\n", what, got);
    }
}

#define NSWBOOT0 (1u << 26)
#define NBOOT0   (1u << 27)

int main(void)
{
    puts("clock tree (values proven by static assertion at build time)");
    check_u32("PLL input, must be 2.66-16 MHz per DS12288 Table 46", PLL_IN_HZ, 4000000u);
    check_u32("PLL VCO, must be 96-344 MHz", PLL_VCO_HZ, 340000000u);
    check_u32("SYSCLK", PLL_R_HZ, 170000000u);
    check_u32("SYSCLK agrees with board_config.h", PLL_R_HZ, SYSCLK_HZ);
    check("170 MHz requires Range 1 Boost mode", CORE_NEEDS_BOOST_MODE, true);

    puts("\nCAN bit timing (1 Mbit/s Haltech bus)");
    check_u32("time quanta per bit", CAN_TQ_PER_BIT, 34u);
    check_u32("reconstructed bit rate",
              FDCAN_KERNEL_CLK_HZ / (CAN_BRP * CAN_TQ_PER_BIT), CAN_BITRATE_HZ);
    check("sample point within CiA's 75-80%",
          CAN_SAMPLE_POINT_PERMILLE >= 750u && CAN_SAMPLE_POINT_PERMILLE <= 800u, true);
    printf("        sample point = %u.%u%%\n",
           CAN_SAMPLE_POINT_PERMILLE / 10u, CAN_SAMPLE_POINT_PERMILLE % 10u);

    puts("\nboot option bytes (defects 1.3 / 1.6)");
    /* Correct provisioning: nSWBOOT0 = 0, nBOOT0 = 1. */
    check("nSWBOOT0=0 nBOOT0=1  -> safe",            boot_option_bytes_ok(NBOOT0), true);
    check("nSWBOOT0=1 nBOOT0=1  -> BOOT0 from pin",  boot_option_bytes_ok(NSWBOOT0 | NBOOT0), false);
    check("nSWBOOT0=0 nBOOT0=0  -> boots bootloader",boot_option_bytes_ok(0u), false);
    check("nSWBOOT0=1 nBOOT0=0  -> both wrong",      boot_option_bytes_ok(NSWBOOT0), false);
    /* Factory-fresh parts have all option bits set. This is the state a mass
     * erase restores, and it is UNSAFE -- the case that must never pass. */
    check("factory default 0xFFFFFFFF -> unsafe",    boot_option_bytes_ok(0xFFFFFFFFu), false);
    /* Other bits in OPTR must not influence the verdict. */
    check("unrelated OPTR bits ignored",
          boot_option_bytes_ok(NBOOT0 | 0x00FFFFFFu), true);

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
