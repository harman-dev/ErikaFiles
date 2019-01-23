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

#include "ee_internal.h"
#include "bcm/drivers/inc/vic.h"
#include "bcm/drivers/inc/uart.h"
#include "bcm/drivers/inc/eth.h"
#include "bcm/drivers/inc/flash.h"
#include "bcm/utils/inc/shell.h"
#include "bcm/utils/inc/build_info.h"
#include "bcm/utils/inc/log.h"
#include "mcu/bcm/common/inc/bcm_info.h"
#include "mcu/bcm/common/inc/mpu.h"
#include "mcu/bcm/common/inc/cortex_rx_mpu.h"
#include "mcu/bcm/common/inc/bcm_timer.h"

#ifdef __ENABLE_FLASH_UPDATER__
#include "bcm/utils/inc/ptm.h"
#include "bcm/utils/inc/flash_updater.h"

extern void fup_init(void);
#endif

const mpu_table_t mpu_table_mcu[MPU_TABLE_SIZE_MCU] = {
/**********************************************************************/
/*   region_base_addr         SIZE         region_attr
**********************************************************************/
{ ATCM_START_ADDR, MPU_SIZE_256K, CACHED_NONSHARED_RW_REGION },
{ B0TCM_START_ADDR, MPU_SIZE_256K, CACHED_NONSHARED_RW_REGION },
{ BOOTROM_START_ADDR, MPU_SIZE_32K, NONCACHED_NONSHARED_RO_REGION},
{ SYS_CFG_START_ADDR, MPU_SIZE_512K, STRONGLY_ORDERED_REGION },
{ MEM_TOP_START_ADDR, MPU_SIZE_64K, STRONGLY_ORDERED_REGION },
{ QSPI_FLASH_START_ADDR, MPU_SIZE_64M, STRONGLY_ORDERED_REGION },
{ TOP_BRIDGE_START_ADDR, MPU_SIZE_64M, SHARED_STRONGLY_ORDERED_REGION},
};


/**
  * @brief  Setup the microcontroller system
  * @param  None
  * @retval None
  */

void EE_system_init(void)
{
#ifdef __ENABLE_FLASH_UPDATER__
    int32_t retVal;
#endif

    (void)dmu_init();
    irq_init();
    uart_init();
    timer_init();
    board_init();
    log_init();
#ifdef __ENABLE_MALLOC__
    mm_init();
#endif
#ifdef __ENABLE_ETH__
    eth_init();
#endif
    (void)flash_init();
    build_info_min();
    mcu_info();
#ifdef __ENABLE_FLASH_UPDATER__
    retVal = PTM_Init(&PTM_Cfg);
    if (ERR_OK != retVal) {
        LOG_CRIT("PTM_Init failed:0x%x\n", retVal);
    }
    fup_init();
#endif
}
