/*****************************************************************************
 Copyright 2016-2017 Broadcom Limited.  All rights reserved.

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

#include "board/bcm/989530c1/inc/ee_board.h"
#include "mcu/bcm/common/inc/cortex_rx_mpu.h"
#include "ee.h"

#define TOGGLE_DELAY_NS    500000000ULL
#define LED_MODE_MAP_MASK  0xE0
#define LED_MODE_MAP_OFF   0xE0
#define LED_MODE_MAP_ON    0x1F

#define VECTOR_TBL_REGION_SIZE		MPU_SIZE_1K

#ifdef __ENABLE_FLASH_UPDATER__
static const PTM_LookupTblEntType PTM_LookupTbl[] = {
    {
        .flashID = 0UL,
        .flashAddr = 0x20000UL,
    },
};

PTM_CfgType PTM_Cfg = {
    .ptLoadAddr = NULL,
    .lookupTbl = PTM_LookupTbl,
    .lookupTblSize = (sizeof(PTM_LookupTbl)/sizeof(PTM_LookupTblEntType)),
};
#endif

const mpu_table_t mpu_table_board[MPU_TABLE_SIZE_BOARD] = {
	{VECTOR_TBL_REGION_START, VECTOR_TBL_REGION_SIZE, NONCACHED_NONSHARED_RO_REGION},
};

void board_init(void)
{

}

void mos_blink_led_on(void) {

	EE_UINT16 val = 0U;
    int32_t ret;

	ret = switch_reg_read16((uint32_t)SWITCH_PAGE_00_LED_MODE_MAP_1, &val);
    if (ret == ERR_OK) {
	    val = (val & LED_MODE_MAP_MASK) | LED_MODE_MAP_OFF;
	    ret = switch_reg_write16((uint32_t)SWITCH_PAGE_00_LED_MODE_MAP_1, val);
        if(ret != ERR_OK) {
            LOG_DEBUG("%s: SWITCH_PAGE_00_LED_MODE_MAP_1 write failed\n",
                        __func__);
        }
    } else {
        LOG_DEBUG("%s: SWITCH_PAGE_00_LED_MODE_MAP_1 read failed\n",
                   __func__);
    }

	ret = switch_reg_read16((uint32_t)SWITCH_PAGE_00_LED_MODE_MAP_0,  &val);
    if (ret == ERR_OK) {
	    val = (val & LED_MODE_MAP_MASK) |LED_MODE_MAP_ON;
	    ret = switch_reg_write16((uint32_t)SWITCH_PAGE_00_LED_MODE_MAP_0,  val);
        if(ret != ERR_OK) {
            LOG_DEBUG("%s: SWITCH_PAGE_00_LED_MODE_MAP_0 write failed\n",
                        __func__);
        }
    } else {
        LOG_DEBUG("%s: SWITCH_PAGE_00_LED_MODE_MAP_0 read failed\n",
                   __func__);
    }
	ndelay(TOGGLE_DELAY_NS);
}

void mos_blink_led_off(void)
{
	EE_UINT16 val = 0U;
    int32_t ret;

	ret = switch_reg_read16((uint32_t)SWITCH_PAGE_00_LED_MODE_MAP_0, &val);
    if (ret == ERR_OK) { 
	    val = (val & LED_MODE_MAP_MASK) | LED_MODE_MAP_OFF;
	    ret = switch_reg_write16((uint32_t)SWITCH_PAGE_00_LED_MODE_MAP_0, val);
        if(ret != ERR_OK) {
            LOG_DEBUG("%s: SWITCH_PAGE_00_LED_MODE_MAP_0 write failed\n",
                        __func__);
        }
    } else {
        LOG_DEBUG("%s: SWITCH_PAGE_00_LED_MODE_MAP_0 read failed\n",
                   __func__);
    }
	ndelay(TOGGLE_DELAY_NS);
}

void toggle_led(void)
{
	static uint32_t val = 0UL;
	val++;
	if(1024UL == val) {
		LOG_DEBUG("Alarm Trigger: Toggle Led is not supported \n");
		val = 0UL;
    }

}

void platform_sleep() {
    sleep(1);
}
