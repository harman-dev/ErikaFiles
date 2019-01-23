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

#include "ee.h"
#include "bcm/utils/inc/avb_utils.h"
#include "bcm/drivers/inc/vic.h"
#include "robo-mgmt.h"

#define SOFT2VIC_INTR VIC_INT_07_NUM

/*
 * SW spare registers 14 & 15 are used to exchange command and reply buffer
 * information respectively between Host and Leo.
 *		SW_SPARE14/SW_SPARE15 field description:
 *			[15:1]	- Buffer Address[19:5]
 *					  Addressable range : 0x0000_0020 - 0x000f_0000
 *					  Alignment : 32byte
 *			[0]		- Queued status
 *					  1 : Queued
 *					  0 : Available
 */
#define MSG_BUFFER					0xfffe
#define MSG_BUFFER_SHIFT			0x4

/* 512bytes each for command and reply buffers at the end of TCM */
extern uint8_t IPC_BUFFER_POOL_START[];
#define REPLY_BUFFER_ADDR	(IPC_BUFFER_POOL_START + CMD_BUFFER_MAX) /* 0x7FE00 */
#define CMD_BUFFER_ADDR		(IPC_BUFFER_POOL_START) /* 0x7FC00 */

/*@
 * mgmt_data - the ROBO Management datastructure
 */
static mgmt_t mgmt_data;

ISR2(soft2vic_irq_handler)
{
    VIC_INT_DISABLE(SOFT2VIC_INTR);
    SetEvent(MgmtCmdHandler, CmdEvent);
}

TASK(MgmtCmdHandler)
{
    EE_UINT32 mask;
    uint32_t reg_val;

    while (1) {
        WaitEvent(CmdEvent);
        GetEvent(MgmtCmdHandler, &mask);
        if ((mask & CmdEvent) == 0UL) {
            LOG_ERROR("Spurious event, Chk this!");
        }
        ClearEvent(CmdEvent);

        if ((reg_read16(MISC_SPARE_SW_REG14) & MSG_QUEUED) != 0U) {
            raiseMgmtCallback(mgmt_data);
        } else {
            LOG_DEBUG("Cmd not queued by host\n");
        }
        /* Host set bit CFG_CPU_INTR_FORCE[7] to interrupt bcm8953x's cpu
         * and hence clearing the source, assuming that the host doesn't queque
         * multiple requests at a time
         */
        reg_val = reg_read32(CFG_CPU_INTR_FORCE);
        reg_val &= ~(1<<7);
        reg_write32(CFG_CPU_INTR_FORCE, reg_val);

        /* re-enable interrupt line */
        VIC_INT_ENABLE(SOFT2VIC_INTR);

        /* Let host know that cmd is processed */
        reg_write16(MISC_SPARE_SW_REG14, reg_read16(MISC_SPARE_SW_REG14) & MSG_BUFFER);

         /* Let host know that reply is available
         Todo: Is reply available to all commands? If so, spare14 is
         sufficient for synchronisation, else update only if reply is true */
         reg_write16(MISC_SPARE_SW_REG15, reg_read16(MISC_SPARE_SW_REG15) | MSG_QUEUED);
   }
}




#ifdef STATS
/*@
 * mgmt_stats - one block of stats
 */
static struct {
    int         command_packets;
} mgmt_stats;

static stats_entry_t mgmt_stats_array[] = {
    { "command packets", &mgmt_stats.command_packets, NULL },
};

static stats_group_t mgmt_group = {
    "ROBO Management",
    sizeof(mgmt_stats_array)/sizeof(mgmt_stats_array[0]),
    mgmt_stats_array
};


void AVBIncrementStat(int val)
{
    mgmt_stats.command_packets += val;
}
#else
void AVBIncrementStat(int val)
{
}
#endif


/*@api
 * mgmt_init
 *
 * @brief
 * ROBO management initialization code
 *
 * @param=priority - priority at which to create the ROBO management init thread
 * @returns void
 *
 * @desc
 *
 * Initialize the ROBO management layer.
 */
void mgmt_init(int priority)
{
    mgmt_t *mgmt = &mgmt_data;

#ifdef STATS
    /* Initialize the statistics */
    stats_add_group(&mgmt_group);
#endif

    /* Reserved memory at the end of TCM */
    mgmt->cmd_buffer = (struct mgmt_command_s *)CMD_BUFFER_ADDR;
    mgmt->reply_buffer = (struct mgmt_reply_s *)REPLY_BUFFER_ADDR;
    reg_write16(MISC_SPARE_SW_REG14, (uint32_t)mgmt->cmd_buffer >> MSG_BUFFER_SHIFT);
    reg_write16(MISC_SPARE_SW_REG15, (uint32_t)mgmt->reply_buffer >> MSG_BUFFER_SHIFT);

    ActivateTask(MgmtCmdHandler);

}
