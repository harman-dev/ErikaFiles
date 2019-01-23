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

#include <string.h>
#include <stdint.h>
#include "bcm/drivers/inc/gpio.h"
#include "bcm/utils/inc/debug.h"
#include "ee_internal.h"

void gpio_config_output(gio_group grp, int pin)
{
	uint32_t base = 0UL;
	uint32_t reg;

	switch (grp) {
	case GIO_GROUP_0:
	case GIO_GROUP_2:
		break;
	case GIO_GROUP_1:
		base = (uint32_t)GIO0_GPIO_G1_DRV_EN;
		break;
	default:
        DBG_ASSERT( 0UL, "gpio_config_output: Wrong GPIO group chosen\n");
		break;
	}

	if (base != 0UL) {
		reg = reg_read32(base);
		reg |= ((uint32_t)pin);
		reg_write32(base, reg);
	}
}

void gpio_config_input(gio_group grp, int pin)
{
	uint32_t base = 0UL;
	uint32_t reg;

	switch (grp) {
	case GIO_GROUP_0:
	case GIO_GROUP_2:
		break;
	case GIO_GROUP_1:
		base = (uint32_t)GIO0_GPIO_G1_DRV_EN;
		break;
	default:
        DBG_ASSERT( 0UL, "gpio_config_input: Wrong GPIO group chosen\n");
		break;
	}

	if (base != 0UL) {
		reg = reg_read32(base);
		reg &= ~((uint32_t)pin);
		reg_write32(base, reg);
	}
}

void gpio_set(gio_group grp, int pin, int val)
{
	uint32_t base = 0UL;
	uint32_t reg;

	switch (grp) {
	case GIO_GROUP_0:
	case GIO_GROUP_2:
		break;
	case GIO_GROUP_1:
		base = (uint32_t)GIO0_GPIO_G1_DOUT;
		break;
	default:
        DBG_ASSERT( 0UL, "gpio_set: Wrong GPIO group chosen\n");
		break;
	}

	if (base != 0UL) {
		reg = reg_read32(base);
		if (val == 1L)
			reg |= (uint32_t)pin;
		else
			reg &= ~((uint32_t)pin);
		reg_write32(base, reg);
	}
}

int gpio_get(gio_group grp, int pin)
{
	uint32_t base = 0UL;
	int val = 0L;

	switch (grp) {
	case GIO_GROUP_0:
	case GIO_GROUP_2:
		break;
	case GIO_GROUP_1:
		base = (uint32_t)GIO0_GPIO_G1_DIN;
		break;
	default:
        DBG_ASSERT( 0UL, "gpio_get: Wrong GPIO group chosen\n");
		break;
	}

	if (base != 0UL) {
		val = ((reg_read32(base) & ((uint32_t)pin)) != 0UL)? 1L : 0L;
	}
	return val;
}

void gpio_flash_cs_din_en(int enable)
{
	if (enable != 0L)
		reg_write32((uint32_t)GIO0_FLASH_CS_DIN, 0x1);
	else
		reg_write32((uint32_t)GIO0_FLASH_CS_DIN, 0x0);
}

void gpio_flash_cs_dout_en(int enable)
{
	if (enable != 0L)
		reg_write32((uint32_t)GIO0_FLASH_CS_DOUT, 0x1);
	else
		reg_write32((uint32_t)GIO0_FLASH_CS_DOUT, 0x0);
}

void gpio_flash_cs_oe_en(int enable)
{
	if (enable != 0L)
		reg_write32((uint32_t)GIO0_FLASH_CS_OE, 0x1);
	else
		reg_write32((uint32_t)GIO0_FLASH_CS_OE, 0x0);
}

void gpio_flash_mux_sel(flash_cs_mux mux)
{
	if (mux == FLASH_CS_FUNC_CS)
		reg_write32((uint32_t)GIO0_GPIO_G2_FLASH_SEL, 0x1);
	else if (mux == FLASH_CS_FUNC_GPIO)
		reg_write32((uint32_t)GIO0_GPIO_G2_FLASH_SEL, 0x0);
}
