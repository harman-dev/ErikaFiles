/*****************************************************************************
 Copyright 2016 Broadcom Limited.  All rights reserved.

 This program is the proprietary software of Broadcom Limited and/or its
 licensors, and may only be used, duplicated, modified or distributed pursuant
 to the terms and conditions of a separate, written license agreement executed
 between you and Broadcom (an "Authorized License").

 Except as set forth in an Authorized License, Broadcom grants no license
 (express or implied), right to use, or waiver of any kind with respect to the
 Software, and Broadcom expressly reserves all rights in and to the Software
 and all intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED
 LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD
 IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

  Except as expressly set forth in the Authorized License,
 1. This program, including its structure, sequence and organization,
    constitutes the valuable trade secrets of Broadcom, and you shall use all
    reasonable efforts to protect the confidentiality thereof, and to use this
    information only in connection with your use of Broadcom integrated
    circuit products.

 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS,
    QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION.
    YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE
    SOFTWARE.

 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

/* Includes */

#include <stdint.h>
#include <io.h>
#include <print.h>
#include "pl022.h"

#define SPI_DBG
/* Macros */
#define PL022_REG(pl022_dev)	((volatile pl022_t *)pl022_dev->regs)

#define SPP_BUSY(regs)		(regs->sspsr & SPI0_F_BSY_MASK)
#define TX_FIFO_FULL(regs)	(!(regs->sspsr & SSPSR_TNF_MASK))
#define TX_FIFO_EMPTY(regs)	(regs->sspsr & SPI0_F_TFE_MASK)

#define RX_FIFO_FULL(regs)	(regs->sspsr & SPI0_F_RFF_MASK)
#define RX_FIFO_EMPTY(regs)	(!(regs->sspsr & SSPSR_RNE_MASK))

#define WAIT_TX_NOT_FULL(dev)	\
	while (!(PL022_REG(dev)->sspsr & SSPSR_TNF_MASK));

#define WAIT_RX_NOT_EMPTY(dev)		\
	while (!(PL022_REG(dev)->sspsr & SSPSR_RNE_MASK));


#ifdef SPI_DBG
#define spi_trace(...)		printf(__VA_ARGS__)
#else
#define spi_trace(...)
#endif

/* Variable definitions */

static pl022_dev_t pl022_devices[NUM_PL022];

/* Prototypes and Externs */

/* Implementation */

static void pl022_hardware_init(pl022_dev_t *device, uint32_t clock, uint32_t cpsr_min)
{
    uint32_t sod = 0;   /* slave-mode output disable */
    uint32_t sse = 1;   /* enable */
    uint32_t lbm = 0;   /* loopback */

    uint32_t half_divisor = (clock + (device->speed * 2 - 1)) / (device->speed*2);
    uint32_t best_err = ~0;
    uint32_t best_scr = PL022_SCR_MAX;
    uint32_t best_half_cpsr = PL022_CPSR_MAX/2;
    uint32_t scr, half_cpsr, err;

    spi_trace("spi%d: hwconfig clock=%d speed=%d format=%u bits=%d\n",
              device->spi, clock, device->speed, device->format, device->bits);

    /*
     * Loop over possible SCR values, calculating the appropriate CPSR and
     * finding the best match.
     * For any SPI speeds above 260KHz, the first iteration will be it, and it will stop.
     * The loop is left in for completeness
     */
    spi_trace("spi%d: setting up PL022 for: %dHz, (target %d)\n",
              device->spi, device->speed, half_divisor);

    for (scr = PL022_SCR_MIN; scr <= PL022_SCR_MAX; ++scr) {
        /* find the right cpsr (rounding up) for the given scr */
        half_cpsr = ((half_divisor + scr) / (1+scr));
        if (half_cpsr < cpsr_min/2) {
            half_cpsr = cpsr_min/2;
        }
        if (half_cpsr > cpsr_min/2) {
            continue;
        }

        err = ((1 + scr) * half_cpsr) - half_divisor;
        spi_trace("spi%d: scr=%d half_cpsr=%d err=%d best_err=%d\n",
                  device->spi, scr, half_cpsr, err, best_err);
        if (err < best_err) {
            best_err = err;
            best_scr = scr;
            best_half_cpsr = half_cpsr;
            if (err == 0) {
                break;
            }
        }
    }

    spi_trace("spi%d: actual clock rate: %dHz\n", device->spi,
              clock/(2 * best_half_cpsr * (1+best_scr)));
    spi_trace("spi%d: best_scr=0x%02x\n", device->spi, best_scr);
    spi_trace("spi%d: best_half_cpsr=0x%02x\n", device->spi, best_half_cpsr);
    spi_trace("spi%d: phase=%d\n", device->spi, device->phase);
    spi_trace("spi%d: polarity=%d\n", device->spi, device->polarity);

    /* Power off module */
    PL022_REG(device)->sspcr1 = ((sod << SSPCTRL1_SOD_SHIFT)
                            | (device->ms << SSPCTRL1_MS_SHIFT)
                            | (0 << SSPCTRL1_SSE_SHIFT)
                            | (lbm << 0));

    /* Set CR0 params */
    PL022_REG(device)->sspcr0 = ((best_scr << SSPCTRL0_SCR_SHIFT)
                            | (device->phase << SSPCTRL0_SPH_SHIFT)
                            | (device->polarity << SSPCTRL0_SPO_SHIFT)
                            | (device->format << SSPCTRL0_FRF_SHIFT)
                            | ((device->bits - 1) << SSPCTR0_DSS_SHIFT));

    /* Set prescale divisor */
    PL022_REG(device)->sspcpsr = best_half_cpsr * 2;

    /* Power on module */
    PL022_REG(device)->sspcr1 = ((sod << SSPCTRL1_SOD_SHIFT)
                            | (device->ms << SSPCTRL1_MS_SHIFT)
                            | (sse << SSPCTRL1_SSE_SHIFT)
                            | (lbm << 0));
}

void pl022_byte_out(pl022_dev_t *device, uint8_t out)
{
    /* Wait for TX not full */
    WAIT_TX_NOT_FULL(device);

    /* send output byte */
    PL022_REG(device)->sspdr = out;
}

uint8_t pl022_byte_in(pl022_dev_t *device)
{
    uint32_t data;

    /* Wait for RX not empty */
    WAIT_RX_NOT_EMPTY(device);

    /* read in byte */
    data = PL022_REG(device)->sspdr;
    spi_trace("spi: DIN: 0x%02x\n", data & 0xff);
    return data;
}

void pl022_byte_operation(pl022_dev_t *device, uint8_t out, uint8_t *in)
{
    uint32_t data;

    /* Wait for TX not full */
    WAIT_TX_NOT_FULL(device);
    /* send output byte */
    PL022_REG(device)->sspdr = out;

    /* Wait for RX not empty */
    WAIT_RX_NOT_EMPTY(device);
    /* read in byte */
    data = PL022_REG(device)->sspdr;

    spi_trace("spi: DOUT:0x%02x DIN:0x%02x\n", out, data & 0xff);

    *in = data;
}

void pl022_transfer(pl022_dev_t *device, char *out, uint32_t out_valid,
		uint32_t in_valid, char *in, uint32_t len)
{
	uint32_t txlen = len;
	uint32_t rxlen = len;

	while ((txlen > 0) || (rxlen > 0)) {

		/* Output a byte */
		if ((txlen > 0) &&
			(PL022_REG(device)->sspsr & SSPSR_TNF_MASK)) {
			/* send output byte */
			PL022_REG(device)->sspdr = out_valid ? *out++ : 0;
			txlen--;
		}
		/* Drain the RX FIFO */
		while ((rxlen > 0) &&
				(PL022_REG(device)->sspsr & SSPSR_RNE_MASK)) {
			uint32_t data;

			/* read in byte */
			data = PL022_REG(device)->sspdr;
			if (in_valid) {
				*in++ = data;
			}
			rxlen--;
		}
	}
}
void pl022_start(pl022_dev_t *device)
{
//	uint32_t regval;

	switch (device->spi) {
	case SPI_FLASH:
		/* To be confirm with ASIC team
		 * Not sure why this code is present in Polar
		 * Register 0xcc828 doesn't have any clear
		 * description and it does not even exits in
		 * polar RDB
		 */
#if 0
		regval = readl(GIO0_R_GRPF0_AUX_SEL_MEMADDR);
		regval &= 0xfffffeff;
		writel(GIO0_R_GRPF0_AUX_SEL_MEMADDR, regval);
#endif
		writel(GIO0_FLASH_CS_OE, 0xffffffff);
		writel(GIO0_FLASH_CS_DOUT, 0x00000000);
		break;
	}
}
void pl022_stop(pl022_dev_t *device)
{
	switch (device->spi) {
	case SPI_FLASH:
		writel(GIO0_FLASH_CS_OE, 0xffffffff);
		writel(GIO0_FLASH_CS_DOUT, 0xffffffff);
		break;
	}
}

pl022_dev_t *pl022_device(int spi)
{
    return &pl022_devices[spi];
}

void pl022_configure(int spi, uint32_t ms, uint32_t clock,
		uint32_t speed, uint32_t cpsr_min)
{

	pl022_dev_t *device;

	if (spi > NUM_PL022) {
		spi_trace("ERROR: invalid spi number\n");
		return;
	}

	device = &pl022_devices[spi];
	device->spi = spi;
	device->regs = PL022_BASE(spi);

	device->ms = ms;
	device->speed = speed;
	device->format = SPI_MOTOROLA;
	device->polarity = 0;
	device->phase = 0;
	device->bits = 8;

	pl022_hardware_init(device, clock, cpsr_min);
}

/* END OF FILE */
