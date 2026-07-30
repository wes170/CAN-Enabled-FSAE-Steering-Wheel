/* display_dash.h — Riverdi RVT50HQBNWN00, 800x480, BT817Q "EVE4" coprocessor.
 *
 * Completely unlike the wheel panel, and worth stating plainly because both
 * boards run one codebase: the wheel drives a dumb memory-in-pixel panel
 * pixel-by-pixel at 2 MHz, while this module contains a graphics coprocessor
 * you send *drawing commands* to at up to 30 MHz. There is no framebuffer in
 * the MCU here. Do not carry habits from `display_wheel.c` across.
 *
 * Sources:
 *   BT81X datasheet BRT_000220 v1.0 — SPI protocol §4.1.2-4.1.5, memory map §5
 *   Riverdi RVT50HQBNWN00 DS Rev 1.7 — panel timing register values
 */
#ifndef DISPLAY_DASH_H
#define DISPLAY_DASH_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#ifdef BOARD_DASH

/* ═══ SPI transaction prefixes (BT81X §5) ═══
 *
 * "All memory and registers in the BT815/6 core are memory mapped in 22-bit
 *  address space with a 2-bit SPI command prefix. Prefix 0b00 for read and
 *  0b10 for write to the address space, 0b01 is reserved for Host Commands
 *  and 0b11 undefined."
 *
 * Serial data is most-significant-bit first.
 *
 * ⚠ A READ HAS A DUMMY BYTE, A WRITE DOES NOT. Read is
 * [00|addr21:16][addr15:8][addr7:0][dummy] then data; write is the same three
 * address bytes then data immediately. Forgetting the dummy byte shifts every
 * byte read back by one, which produces garbage that looks like a wiring
 * fault rather than a protocol one. */
#define EVE_PREFIX_READ  0x00u
#define EVE_PREFIX_WRITE 0x80u   /* 0b10 in the top two bits */
#define EVE_PREFIX_CMD   0x40u   /* 0b01 */

/* ═══ Memory map (BT81X §5) ═══ */
#define EVE_RAM_G      0x000000u   /* 1 MB general-purpose graphics RAM */
#define EVE_ROM        0x200000u
#define EVE_RAM_DL     0x300000u   /* 8 kB display list                 */
#define EVE_RAM_REG    0x302000u   /* 4 kB registers                    */
#define EVE_RAM_CMD    0x308000u   /* 4 kB coprocessor command FIFO     */
#define EVE_RAM_CMD_SIZE 4096u

/* ═══ Registers ═══ */
#define EVE_REG_ID          0x302000u   /* r/o, always reads 0x7C */
#define EVE_ID_VALUE        0x7Cu

/*  Timing register addresses, every one read from the BT81X register table.
 *  They are NOT evenly derivable from a base: the table has gaps, and an
 *  earlier draft of this driver guessed REG_HSIZE at 0x30202C when it is
 *  actually 0x302034 — an off-by-two-slots error that would have written the
 *  horizontal size into REG_HCYCLE and the vertical size into REG_HOFFSET.
 *  The display would have lit up and shown a torn image. Do not infer these. */
#define EVE_REG_HCYCLE      0x30202Cu
#define EVE_REG_HOFFSET     0x302030u
#define EVE_REG_HSIZE       0x302034u
#define EVE_REG_HSYNC0      0x302038u
#define EVE_REG_HSYNC1      0x30203Cu
#define EVE_REG_VCYCLE      0x302040u
#define EVE_REG_VOFFSET     0x302044u
#define EVE_REG_VSIZE       0x302048u
#define EVE_REG_VSYNC0      0x30204Cu
#define EVE_REG_VSYNC1      0x302050u
#define EVE_REG_DLSWAP      0x302054u
#define EVE_REG_ROTATE      0x302058u
#define EVE_REG_DITHER      0x302060u
#define EVE_REG_SWIZZLE     0x302064u
#define EVE_REG_CSPREAD     0x302068u
#define EVE_REG_PCLK_POL    0x30206Cu
#define EVE_REG_PCLK        0x302070u
#define EVE_REG_CPURESET    0x302020u

/* ═══ Host commands (BT81X Table 4-5) ═══
 * Sent as three bytes: [01|cmd5:0][parameter][0x00]. */
#define EVE_HOSTCMD_ACTIVE   0x00u
#define EVE_HOSTCMD_STANDBY  0x41u
#define EVE_HOSTCMD_SLEEP    0x42u
#define EVE_HOSTCMD_PWRDOWN  0x43u
#define EVE_HOSTCMD_CLKEXT   0x44u
#define EVE_HOSTCMD_CLKINT   0x48u

/* ═══ Panel timing — Riverdi RVT50HQBNWN00 DS Rev 1.7, "REGISTER VALUES" ═══
 *
 * These are properties of THIS panel, not of the BT817. They cannot be
 * derived, guessed, or carried over from another 800x480 module: a wrong
 * HCYCLE or PCLK_POL gives a display that lights up and shows a skewed,
 * torn or inverted image, which reads as a broken panel. */
#define EVE_HSIZE      800u
#define EVE_VSIZE      480u
#define EVE_HCYCLE     816u
#define EVE_HOFFSET      8u
#define EVE_HSYNC0       0u
#define EVE_HSYNC1       4u
#define EVE_VCYCLE     496u
#define EVE_VOFFSET      8u
#define EVE_VSYNC0       0u
#define EVE_VSYNC1       4u
#define EVE_PCLK         1u
#define EVE_SWIZZLE      0u
#define EVE_PCLK_POL     1u
#define EVE_CSPREAD      0u
#define EVE_DITHER       0u

/*  ⚠ `REG_PCLK = 1` is an EVE4 (BT817/818) convention, not the EVE3 meaning.
 *  The BT815/6 datasheet available here states plainly: "REG_PCLK is the PCLK
 *  divisor… PCLK frequency = System Clock frequency / REG_PCLK", which at
 *  REG_PCLK = 1 would run the panel at the full system clock. On EVE4 it selects the separate
 *  `REG_PCLK_FREQ` register. The datasheet available locally is BRT_000220,
 *  which covers **BT815/6 (EVE3)** — the register map and SPI protocol above
 *  are common to both families, but this one value is where they differ.
 *  [OPEN] Confirm `REG_PCLK_FREQ` against the BT817/818 datasheet or
 *  Riverdi's published init sequence (github.com/riverdi/riverdi-eve) before
 *  bring-up. Symptom if wrong: no image or a badly wrong refresh rate — loud,
 *  not subtle, and safe to discover on the bench. */

/* SPI ceiling for this module. Note it is 15x the wheel panel's. */
#define EVE_SPI_MAX_HZ 30000000u

bool display_dash_init(void);
bool display_dash_is_ready(void);

/* ---------------------------------------------------------------------------
 * Host-testable address/command encoding
 * ------------------------------------------------------------------------ */

/* Build the 3 address bytes for a read (caller then clocks a dummy byte). */
void eve_addr_read(uint32_t addr, uint8_t out[3]);

/* Build the 3 address bytes for a write. */
void eve_addr_write(uint32_t addr, uint8_t out[3]);

/* Build a 3-byte host command. */
void eve_host_cmd(uint8_t cmd, uint8_t parameter, uint8_t out[3]);

/*  Advance a RAM_CMD FIFO write pointer. The FIFO is a 4 kB ring and the
 *  pointer must wrap; letting it run past the end walks into RAM_G and
 *  corrupts graphics data rather than failing cleanly. */
uint16_t eve_cmd_fifo_advance(uint16_t offset, uint16_t bytes);

/* Free space in the command FIFO given the write and read pointers. */
uint16_t eve_cmd_fifo_free(uint16_t write_ptr, uint16_t read_ptr);

#endif /* BOARD_DASH */
#endif /* DISPLAY_DASH_H */
