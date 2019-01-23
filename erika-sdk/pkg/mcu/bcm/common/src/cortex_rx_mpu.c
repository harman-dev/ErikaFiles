/*****************************************************************************
Copyright 2016 Broadcom Limited.  All rights reserved.
This program is the proprietary software of Broadcom Limited and/or its
licensors, and may only be used, duplicated, modified or distributed pursuant
to the terms and conditions of a separate, written license agreement executed
between you and Broadcom (an "Authorized License").

Except as set forth in an Authorized License, Broadcom grants no
license(express or implied), right to use, or waiver of any kind with respect
to the Software, and Broadcom expressly reserves all rights in and to the
Software and all intellectual property rights therein.  IF YOU HAVE NO
AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY,
AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

 Except as expressly set forth in the Authorized License,
1. This program, including its structure, sequence and organization,
constitutes the valuable trade secrets of Broadcom, and you shall use all
reasonable efforts to protect the confidentiality thereof, and to use this
information only in connection with your use of Broadcom integrated circuit
products.

2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" AND
WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR WARRANTIES,
EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO THE SOFTWARE.
BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE,
MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
PERFORMANCE OF THE SOFTWARE.

3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
ANY LIMITED REMEDY.
***************************************************************************/
#include <stdint.h>
#include "mcu/bcm/common/inc/mpu.h"
#include "mcu/bcm/common/inc/cortex_rx_mpu.h"
#include "board/bcm/common/inc/ee_board.h"
#include "mcu/bcm/common/inc/ee_mcu.h"

#define MPU_ENABLE()				\
{						\
	asm volatile (				\
		"MRC p15, 0, r1, c1, c0, 0;"	\
		"ORR r1, r1, #0x1;"		\
		"DSB;"				\
		"MCR p15, 0, r1, c1, c0, 0;"	\
		"ISB;"				\
		:::"r1");			\
}


#define MPU_DISABLE()				\
{						\
	asm volatile (				\
		"MRC p15, 0, r1, c1, c0, 0;"	\
		"BIC r1, r1, #0x1;"		\
		"DSB;"				\
		"MCR p15, 0, r1, c1, c0, 0;"	\
		"ISB;"				\
		:::"r1");			\
}

#define MPU_SET_REG_ID(id)			\
{						\
	asm volatile (				\
		"MCR p15, 0, %0, c6, c2, 0;"	\
		"ISB;"				\
		::"r"(id));			\
}

#define MPU_SET_BASE(base)			\
{						\
	asm volatile (				\
		"MCR p15, 0, %0, c6, c1, 0;"	\
		::"r"(base));			\
}

#define MPU_SET_SZ_ENABLE(size)			\
{						\
	asm volatile (				\
		"LSL %0, %0, #0x1;"		\
		"ORR %0, %0, #0x1;"		\
		"MCR p15, 0, %0, c6, c1, 2;"	\
		::"r"(size));			\
}

#define MPU_SET_ATTRB(attrb)			\
{						\
	asm volatile (				\
		"MCR p15, 0, %0, c6, c1, 4;"	\
		::"r"(attrb));			\
}

void mpu_config()
{
	uint32_t i;
	uint32_t reg_num = 0UL;

#if ((MPU_TABLE_SIZE_BOARD + MPU_TABLE_SIZE_MCU) > MPU_NUM_REGION_MAX)
#error "MPU table entries overflow"
#endif

	MPU_DISABLE();

	for(i = 0UL; i < MPU_TABLE_SIZE_MCU; i++) {
		MPU_SET_REG_ID(reg_num++);
		MPU_SET_BASE(mpu_table_mcu[i].region_base_addr);
		MPU_SET_SZ_ENABLE(mpu_table_mcu[i].region_sz);
		MPU_SET_ATTRB(mpu_table_mcu[i].region_attr);
	}

	for(i = 0UL; i < MPU_TABLE_SIZE_BOARD; i++) {
		MPU_SET_REG_ID(reg_num++);
		MPU_SET_BASE(mpu_table_board[i].region_base_addr);
		MPU_SET_SZ_ENABLE(mpu_table_board[i].region_sz);
		MPU_SET_ATTRB(mpu_table_board[i].region_attr);
	}

	MPU_ENABLE();
}
