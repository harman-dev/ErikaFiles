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


#include <string.h>
#include <stdint.h>
#include "bcm/drivers/inc/qspi.h"
#include "bcm/drivers/inc/crc.h"
#include "ee_internal.h"

/* Macros */
#define CRC_ENABLE_MASK 	0x1UL
#define CRC_CLEAR_MASK		0x2UL

/* Globals */
extern volatile int raf_num_words;

/* Function definitions */

static void clear_crc(void)
{
	uint32_t reg;

	/*
	 * Flash the QSPI BSPI pre-fetch buffer.
	 */
	reg = reg_read32((uint32_t)QSPI_BSPI_REGISTERS_B0_CTRL);
	reg &= (~((uint32_t)QSPI_BSPI_REGISTERS_B0_CTRL_BSPI_REGISTERS_B0_CTRL_B0_FLUSH_MASK));
	reg_write32((uint32_t)QSPI_BSPI_REGISTERS_B0_CTRL, reg);
	reg |= (uint32_t)QSPI_BSPI_REGISTERS_B0_CTRL_BSPI_REGISTERS_B0_CTRL_B0_FLUSH_MASK;
	reg_write32((uint32_t)QSPI_BSPI_REGISTERS_B0_CTRL, reg);


	reg = reg_read32((uint32_t)QSPI_BSPI_REGISTERS_B1_CTRL);
	reg &= (~((uint32_t)QSPI_BSPI_REGISTERS_B1_CTRL_BSPI_REGISTERS_B1_CTRL_B1_FLUSH_MASK));
	reg_write32((uint32_t)QSPI_BSPI_REGISTERS_B1_CTRL, reg);
	reg |= (uint32_t)QSPI_BSPI_REGISTERS_B1_CTRL_BSPI_REGISTERS_B1_CTRL_B1_FLUSH_MASK;
	reg_write32((uint32_t)QSPI_BSPI_REGISTERS_B1_CTRL, reg);

	reg = reg_read32((uint32_t)CFG_SPI_CRC_CONTROL);
	reg |= CRC_CLEAR_MASK;
	reg_write32((uint32_t)CFG_SPI_CRC_CONTROL, reg);
	reg &= (~CRC_CLEAR_MASK);
	reg_write32((uint32_t)CFG_SPI_CRC_CONTROL, reg);
}

int enable_crc(unsigned int enable)
{
	uint32_t reg;
	int ret;

	if (enable > 1UL)
		return ERR_INVAL;

	if (enable == 1UL) {

		clear_crc();

		reg = reg_read32((uint32_t)CFG_SPI_CRC_CONTROL);
		reg |= CRC_ENABLE_MASK;
		reg_write32((uint32_t)CFG_SPI_CRC_CONTROL, reg);
		raf_num_words = 0L;
	} else {
		reg = reg_read32((uint32_t)CFG_SPI_CRC_CONTROL);
		reg &= (~CRC_ENABLE_MASK);
		reg_write32((uint32_t)CFG_SPI_CRC_CONTROL, reg);
	}

	return ERR_OK;
}

int ignore_intial_bytes_for_crc(uint32_t bytes_count)
{
	uint32_t reg;

	/*
	 * Initial 0x28 (= 40) cycles = 8 (command) + 24 (address) + 8 (dummy byte)
	 */
	reg = 0x28UL + (8UL * bytes_count);
	reg_write32((uint32_t)CFG_SPI_CRC_IDLE_CYCLE_COUNT, reg);

	return ERR_OK;
}

extern int get_crc_value(uint32_t *crc_value)
{

	if (crc_value == NULL)
		return ERR_INVAL;

	*crc_value = reg_read32((uint32_t)CFG_SPI_CRC_STATUS);

	return ERR_OK;

}

