/* display_dash.c — BT817Q EVE coprocessor over SPI.
 *
 * The MCU holds no framebuffer for this display. It sends drawing commands to
 * a coprocessor which rasterises them. That is why an 800x480 colour panel is
 * affordable on the same MCU that drives a 176x176 panel pixel-by-pixel.
 */

#include "display_dash.h"
#include <string.h>

#ifdef BOARD_DASH

#if defined(STM32G474xx) || defined(USE_CMSIS_DEVICE)
#  include "stm32g4xx.h"
#else
#  define FIRMWARE_HOST_BUILD 1
#endif

static bool s_ready;

/* ---------------------------------------------------------------------------
 * Address and command encoding — pure
 * ------------------------------------------------------------------------ */

void eve_addr_read(uint32_t addr, uint8_t out[3])
{
    if (!out) { return; }
    /* Prefix 0b00. Masking to 22 bits is not cosmetic: an address with stray
     * high bits would corrupt the prefix and turn a read into a write. */
    out[0] = (uint8_t)(EVE_PREFIX_READ | ((addr >> 16) & 0x3Fu));
    out[1] = (uint8_t)((addr >> 8) & 0xFFu);
    out[2] = (uint8_t)(addr & 0xFFu);
}

void eve_addr_write(uint32_t addr, uint8_t out[3])
{
    if (!out) { return; }
    out[0] = (uint8_t)(EVE_PREFIX_WRITE | ((addr >> 16) & 0x3Fu));
    out[1] = (uint8_t)((addr >> 8) & 0xFFu);
    out[2] = (uint8_t)(addr & 0xFFu);
}

void eve_host_cmd(uint8_t cmd, uint8_t parameter, uint8_t out[3])
{
    if (!out) { return; }
    /* Prefix 0b01 then a 6-bit command code. The third byte is fixed at 0x00
     * by the datasheet -- it is not padding the host may reuse. */
    out[0] = (uint8_t)(EVE_PREFIX_CMD | (cmd & 0x3Fu));
    out[1] = parameter;
    out[2] = 0x00u;
}

/* ---------------------------------------------------------------------------
 * Command FIFO bookkeeping — pure
 * ------------------------------------------------------------------------ */

uint16_t eve_cmd_fifo_advance(uint16_t offset, uint16_t bytes)
{
    /* The FIFO is a 4 kB RING. Running the pointer past the end walks into
     * RAM_G and quietly corrupts graphics data instead of failing. */
    return (uint16_t)((offset + bytes) % EVE_RAM_CMD_SIZE);
}

uint16_t eve_cmd_fifo_free(uint16_t write_ptr, uint16_t read_ptr)
{
    /* One slot is always held back so a completely full ring cannot be
     * mistaken for a completely empty one -- both would show write == read. */
    const uint16_t used = (uint16_t)((write_ptr - read_ptr) % EVE_RAM_CMD_SIZE);
    return (uint16_t)(EVE_RAM_CMD_SIZE - 4u - used);
}

/* ---------------------------------------------------------------------------
 * Bring-up
 * ------------------------------------------------------------------------ */

bool display_dash_is_ready(void) { return s_ready; }

bool display_dash_init(void)
{
    s_ready = false;
#ifdef FIRMWARE_HOST_BUILD
    return false;   /* nothing to talk to on a host build */
#else
    /* PDN low then high releases the module from reset; it is active low and
     * the Riverdi module pulls it up internally with 47 kohm, so it must be
     * driven, not merely left alone. */
    eve_pdn_low();
    delay_us(20000u);
    eve_pdn_high();
    delay_us(20000u);

    /* ACTIVE. The datasheet notes a dummy memory read from address 0, twice,
     * also generates ACTIVE -- the explicit host command is clearer. */
    eve_send_host_cmd(EVE_HOSTCMD_ACTIVE, 0x00u);
    delay_us(30000u);

    /* REG_ID must read 0x7C before anything else is worth attempting. Poll
     * rather than assume: the module can take tens of milliseconds, and every
     * later failure is unintelligible if the chip was never awake. */
    for (unsigned tries = 0; tries < 100u; ++tries) {
        if (eve_read8(EVE_REG_ID) == EVE_ID_VALUE) { s_ready = true; break; }
        delay_us(1000u);
    }
    if (!s_ready) { return false; }

    /* Panel timing. Values from the Riverdi datasheet, addresses from the
     * BT81X register table -- see display_dash.h for both. */
    eve_write16(EVE_REG_HCYCLE,   EVE_HCYCLE);
    eve_write16(EVE_REG_HOFFSET,  EVE_HOFFSET);
    eve_write16(EVE_REG_HSIZE,    EVE_HSIZE);
    eve_write16(EVE_REG_HSYNC0,   EVE_HSYNC0);
    eve_write16(EVE_REG_HSYNC1,   EVE_HSYNC1);
    eve_write16(EVE_REG_VCYCLE,   EVE_VCYCLE);
    eve_write16(EVE_REG_VOFFSET,  EVE_VOFFSET);
    eve_write16(EVE_REG_VSIZE,    EVE_VSIZE);
    eve_write16(EVE_REG_VSYNC0,   EVE_VSYNC0);
    eve_write16(EVE_REG_VSYNC1,   EVE_VSYNC1);
    eve_write8 (EVE_REG_SWIZZLE,  EVE_SWIZZLE);
    eve_write8 (EVE_REG_PCLK_POL, EVE_PCLK_POL);
    eve_write8 (EVE_REG_CSPREAD,  EVE_CSPREAD);
    eve_write8 (EVE_REG_DITHER,   EVE_DITHER);

    /* REG_PCLK goes LAST, deliberately. It is what starts the pixel clock, so
     * writing it before the timing registers drives the panel with whatever
     * defaults happened to be loaded -- a brief burst of wrong timing on a
     * real LCD. Enable the output only once it knows what to output. */
    eve_write8(EVE_REG_PCLK, EVE_PCLK);

    return s_ready;
#endif
}

#endif /* BOARD_DASH */
