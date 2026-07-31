/* system_init.h — startup sequence.
 *
 * Two of the three steps exist because of silicon behaviour that happens before
 * your code runs. Read the comments in system_init.c before reordering them.
 */
#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     clock_ok;       /* HSE started and the PLL locked at SYSCLK_HZ   */
    bool     boot_bytes_ok;  /* nSWBOOT0 == 0 and nBOOT0 == 1 (defects 1.3/1.6) */
    bool     usb_clk_ok;     /* HSI48 running and CRS armed (wheel only)      */
    uint32_t sysclk_hz;      /* what the clock tree was configured for        */
} system_init_result_t;

/* Runs, in order: UCPD dead-battery release, clock tree, boot-option check. */
system_init_result_t system_init(void);

/* Releases the 5.1 kohm dead-battery pull-downs from PB4/PB6 (defect 8.2).
 * MUST be called before any GPIO configuration. Calling it late is not a
 * partial fix -- ENC3_A/EVE_INT will already have been sampled wrong. */
void ucpd_dead_battery_disable(void);

/* HSE -> PLL -> 170 MHz. False if the crystal never started or the PLL never
 * locked; in both cases the board is running on HSI16 and CAN bit timing will
 * be wrong, so callers must not proceed to bring CAN up. */
bool clock_init(void);

/* HSI48 + CRS for the USB 48 MHz domain. NOT derivable from the crystal: no PLL
 * configuration serves 170 MHz SYSCLK and 48 MHz PLLQ at once -- the arithmetic
 * is in clock_config.h (defect 8.17). Wheel only; the dash has no USB.
 *
 * Returns false if HSI48 never reports ready. CRS itself cannot be verified
 * here: it only trims once the host starts sending SOF packets, so "armed" is
 * all that can be checked before enumeration. */
bool usb_clock_init(void);

/* True if this board's boot option bytes are provisioned safely. A false here
 * cannot be repaired in firmware -- boot mode latches during reset. Report it
 * loudly (LED pattern, debug UART) so the board is diagnosable. */
bool boot_guard_check(void);

/* Host-testable predicate behind boot_guard_check(): given a raw FLASH_OPTR
 * value, are the boot option bytes correct? */
bool boot_option_bytes_ok(uint32_t optr);

#endif /* SYSTEM_INIT_H */
