/* test_display_dash.c — host tests for the BT817 EVE encoding.
 *
 * The three things that silently go wrong on an EVE part:
 *   1. Read vs write prefix. One bit apart, and a read encoded as a write
 *      does not fail -- it writes.
 *   2. The register addresses. The table has gaps, so inferring them from
 *      even spacing lands on the wrong register. An earlier draft of this
 *      driver did exactly that.
 *   3. The command FIFO ring wrap. Running past 4 kB walks into RAM_G.
 */
#include <stdio.h>
#include <string.h>
#include "display_dash.h"

static int failures = 0;

static void ck(const char *what, bool ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) { ++failures; }
}

static void ck_u(const char *what, unsigned got, unsigned want)
{
    if (got != want) { printf("  FAIL %-48s got 0x%02X want 0x%02X\n", what, got, want); ++failures; }
    else             { printf("  ok   %-48s = 0x%02X\n", what, got); }
}

static void test_addressing(void)
{
    puts("\nSPI address encoding (prefix 00 read / 10 write / 01 host cmd)");
    uint8_t a[3];

    eve_addr_read(EVE_REG_ID, a);
    ck_u("read REG_ID byte 0 (prefix 00 + addr21:16)", a[0], 0x30u);
    ck_u("read REG_ID byte 1", a[1], 0x20u);
    ck_u("read REG_ID byte 2", a[2], 0x00u);

    eve_addr_write(EVE_REG_ID, a);
    ck_u("write REG_ID byte 0 (prefix 10 + addr21:16)", a[0], 0xB0u);
    ck_u("write REG_ID byte 1", a[1], 0x20u);
    ck_u("write REG_ID byte 2", a[2], 0x00u);

    /* The prefixes differ only in bit 7. A read mistakenly encoded as a write
     * does not error -- it writes to the register. Assert they differ. */
    uint8_t r[3], w[3];
    eve_addr_read(EVE_RAM_CMD, r);
    eve_addr_write(EVE_RAM_CMD, w);
    ck("read and write prefixes differ", r[0] != w[0]);
    ck("and only in the prefix bits", (r[0] & 0x3Fu) == (w[0] & 0x3Fu));

    /* A stray high bit in the address must not corrupt the prefix. */
    eve_addr_write(0xFF302000u, a);
    ck_u("address masked to 22 bits, prefix intact", a[0] & 0xC0u, 0x80u);

    eve_addr_read(0u, a);
    ck_u("address 0 read", a[0], 0x00u);
    eve_addr_write(0u, a);
    ck_u("address 0 write", a[0], 0x80u);

    eve_addr_read(EVE_RAM_G, a);   ck_u("RAM_G  byte 0", a[0], 0x00u);
    eve_addr_read(EVE_RAM_DL, a);  ck_u("RAM_DL byte 0", a[0], 0x30u);
    eve_addr_read(EVE_RAM_CMD, a); ck_u("RAM_CMD byte 0", a[0], 0x30u);
    ck_u("RAM_CMD byte 1", (unsigned)((EVE_RAM_CMD >> 8) & 0xFF), 0x80u);

    eve_addr_read(0u, NULL);  /* must not crash */
    ck("NULL output is safe", true);
}

static void test_host_commands(void)
{
    puts("\nhost commands (3 bytes, third fixed at 0x00)");
    uint8_t c[3];

    eve_host_cmd(EVE_HOSTCMD_ACTIVE, 0x00u, c);
    ck_u("ACTIVE byte 0", c[0], 0x40u);
    ck_u("ACTIVE byte 1", c[1], 0x00u);
    ck_u("ACTIVE byte 2 fixed at 0", c[2], 0x00u);

    eve_host_cmd(EVE_HOSTCMD_SLEEP, 0x00u, c);
    ck_u("SLEEP  byte 0 (0x40 | 0x42 masked to 6 bits)", c[0], 0x42u);

    eve_host_cmd(EVE_HOSTCMD_CLKEXT, 0x00u, c);
    ck_u("CLKEXT byte 0", c[0], 0x44u);

    /* The command field is 6 bits; a larger value must not bleed into the
     * prefix and turn a host command into a memory write. */
    eve_host_cmd(0xFFu, 0x00u, c);
    ck_u("oversized command cannot corrupt the prefix", c[0] & 0xC0u, 0x40u);

    /* Third byte is fixed by the datasheet, not spare padding. */
    eve_host_cmd(EVE_HOSTCMD_ACTIVE, 0xAAu, c);
    ck_u("parameter goes in byte 1", c[1], 0xAAu);
    ck_u("byte 2 stays 0 regardless", c[2], 0x00u);
}

static void test_register_addresses(void)
{
    puts("\nregister addresses (the table has GAPS -- do not infer them)");
    /* Every one of these was read from the BT81X register table. The check
     * that matters: they are not evenly spaced from a base, so any code that
     * computes them arithmetically is wrong. */
    ck_u("REG_HCYCLE",   EVE_REG_HCYCLE   - EVE_RAM_REG, 0x2Cu);
    ck_u("REG_HOFFSET",  EVE_REG_HOFFSET  - EVE_RAM_REG, 0x30u);
    ck_u("REG_HSIZE",    EVE_REG_HSIZE    - EVE_RAM_REG, 0x34u);
    ck_u("REG_HSYNC0",   EVE_REG_HSYNC0   - EVE_RAM_REG, 0x38u);
    ck_u("REG_HSYNC1",   EVE_REG_HSYNC1   - EVE_RAM_REG, 0x3Cu);
    ck_u("REG_VCYCLE",   EVE_REG_VCYCLE   - EVE_RAM_REG, 0x40u);
    ck_u("REG_VSIZE",    EVE_REG_VSIZE    - EVE_RAM_REG, 0x48u);
    ck_u("REG_PCLK_POL", EVE_REG_PCLK_POL - EVE_RAM_REG, 0x6Cu);
    ck_u("REG_PCLK",     EVE_REG_PCLK     - EVE_RAM_REG, 0x70u);

    /* The specific earlier mistake: HSIZE guessed at 0x2C, which is HCYCLE. */
    ck("REG_HSIZE is NOT 0x2C (that is HCYCLE)",
       EVE_REG_HSIZE != EVE_RAM_REG + 0x2Cu);
    ck("DITHER and SWIZZLE are distinct", EVE_REG_DITHER != EVE_REG_SWIZZLE);
}

static void test_panel_timing(void)
{
    puts("\npanel timing (Riverdi values -- properties of THIS panel)");
    ck_u("HSIZE",  EVE_HSIZE, 800u);
    ck_u("VSIZE",  EVE_VSIZE, 480u);
    ck_u("HCYCLE", EVE_HCYCLE, 816u);
    ck_u("VCYCLE", EVE_VCYCLE, 496u);

    /* Sanity: the total cycle must exceed the visible size, or the panel has
     * no blanking interval and cannot sync. */
    ck("HCYCLE > HSIZE (there is a horizontal blanking period)",
       EVE_HCYCLE > EVE_HSIZE);
    ck("VCYCLE > VSIZE (there is a vertical blanking period)",
       EVE_VCYCLE > EVE_VSIZE);
    ck("HOFFSET leaves room after HSYNC1", EVE_HOFFSET >= EVE_HSYNC1);
    ck("VOFFSET leaves room after VSYNC1", EVE_VOFFSET >= EVE_VSYNC1);

    /* Pixels per frame, and the clock a 60 Hz refresh would need. */
    const uint32_t px = (uint32_t)EVE_HCYCLE * EVE_VCYCLE;
    printf("        %u x %u = %u pixel clocks per frame; 60 Hz needs %u.%u MHz\n",
           EVE_HCYCLE, EVE_VCYCLE, px, (px * 60u) / 1000000u,
           ((px * 60u) / 100000u) % 10u);
    ck("frame fits a sane pixel clock (< 40 MHz at 60 Hz)", px * 60u < 40000000u);
}

static void test_cmd_fifo(void)
{
    puts("\ncommand FIFO ring (4 kB; overrunning it walks into RAM_G)");
    ck_u("FIFO is 4 kB", EVE_RAM_CMD_SIZE, 4096u);

    ck_u("advance within the ring", eve_cmd_fifo_advance(0u, 100u), 100u);
    ck_u("advance to the last slot", eve_cmd_fifo_advance(4092u, 4u), 0u);
    ck_u("WRAPS rather than overrunning", eve_cmd_fifo_advance(4000u, 200u), 104u);
    ck_u("a full lap returns to the start", eve_cmd_fifo_advance(0u, 4096u), 0u);

    /* Free space. One slot is held back so a full ring cannot be mistaken for
     * an empty one -- both would show write == read. */
    ck("empty ring reports nearly all free",
       eve_cmd_fifo_free(0u, 0u) >= EVE_RAM_CMD_SIZE - 8u);
    ck("free space never reports the entire ring",
       eve_cmd_fifo_free(0u, 0u) < EVE_RAM_CMD_SIZE);
    ck_u("half full", eve_cmd_fifo_free(2048u, 0u), EVE_RAM_CMD_SIZE - 4u - 2048u);
    /* Wrapped pointers must still give a sane answer. */
    ck("wrapped pointers give a sane free count",
       eve_cmd_fifo_free(100u, 4000u) < EVE_RAM_CMD_SIZE);
}

int main(void)
{
    test_addressing();
    test_host_commands();
    test_register_addresses();
    test_panel_timing();
    test_cmd_fifo();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
