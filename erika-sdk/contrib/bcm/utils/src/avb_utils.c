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


/******************************************************************************
 File Name:  avb_utils.c
 Descritpion: This file provides functions that map avb tasks/events and
 resources to erika
******************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include "bcm/utils/inc/avb_utils.h"
#include "bcm/utils/inc/log.h"
#include "cpu/cortex_rx/inc/ee_cpu.h"
#include "eecfg.h"

/******************************************************************************
 FUNCTION NAME: GetOSTaskId

 ARGUMENTS:
 avbTaskId -> enum of the task required

 DESCRIPTION: Returns the task id corresponding to avbTaskId  which gets
 defined in Erika

 RETURNS: taskId
******************************************************************************/

int GetOSTaskId(EAVBTaskId avbTaskId)
{
    int taskId;

    switch (avbTaskId) {
    case eEthIsrHandler:
        taskId = EthIsrHandler;
        break;
    case eAVBThread:
        taskId = AVBThread;
        break;
    case eMgmtCmdHandler:
        taskId = MgmtCmdHandler;
        break;
#ifdef SAVE_CONFIG_PARAMS
    case eConfigSaveThread:
        taskId = ConfigSaveThread;
        break;
#endif
    default:
        LOG_ERROR("Invalid Task \n");
        taskId = -1;
    }

    return taskId;
}

/******************************************************************************
 FUNCTION NAME: GetOSEventId

 ARGUMENTS:
 avbEventId -> enum of the Evnet required

 DESCRIPTION: Returns the event id corresponding to avbEventId  which gets
 defined in Erika

 RETURNS: eventId
******************************************************************************/

int GetOSEventId(EAVBEventId avbEventId)
{
    int eventId;

    switch (avbEventId) {

        case eCmdEvent:
            eventId = CmdEvent;
            break;
        case eEthIsrEvent :
            eventId = EthIsrEvent;
            break;
        case eReceiveEvent:
            eventId = ReceiveEvent;
            break;
        case eSleepEvent:
            eventId = SleepEvent;
            break;
        case eTransmitEvent:
            eventId = TransmitEvent;
            break;
        case eUartEvent:
            eventId = UartEvent;
            break;
        default:
            LOG_ERROR("Invalid Event requested \n");
            eventId = -1;
    }

    return eventId;

}


/******************************************************************************
 FUNCTION NAME: GetOSResourceId

 ARGUMENTS:
 avbResourceId -> enum of the resource required

 DESCRIPTION: Returns the resource id corresponding to avbResourceId  which gets
 defined in Erika

 RETURNS: resId
******************************************************************************/
int GetOSResourceId(EAVBResourceId avbResourceId)
{
    int resId;

    switch (avbResourceId) {
    case eMallocLock:
        resId = MallocLock;
        break;
#ifdef SAVE_CONFIG_PARAMS
    case eConfigSaveResource:
        resId = ConfigSaveResource;
        break;
#endif
    case eTimeStampResource:
        resId = TimeStampResource;
        break;
    case eAsyncNotificationResource:
        resId = AsyncNotificationResource;
        break;
    default :
        resId = -1;
        LOG_ERROR("Invalid Resource requested \n");
    }

    return resId;
}

#define IRQ_SW_LINK_ALL_MASK        ((1UL << IRQ_SWITCH_LINK0) |    \
                                     (1UL << IRQ_SWITCH_LINK1) |    \
                                     (1UL << IRQ_SWITCH_LINK2) |    \
                                     (1UL << IRQ_SWITCH_LINK3) |    \
                                     (1UL << IRQ_SWITCH_LINK4) |    \
                                     (1UL << IRQ_SWITCH_LINK5) |    \
                                     (1UL << IRQ_SWITCH_LINK6) |    \
                                     (1UL << IRQ_SWITCH_LINK7) |    \
                                     (1UL << IRQ_SWITCH_LINK8))

#define INTR_CLR_LINK_ALL           (IRQ_SW_LINK_ALL_MASK >> IRQ_SWITCH_BASE)

/* Assuming that nested interrupts are not supported, a single interrupt handler
   is sufficient to handle all link state changes. Anyway
   portLinkStateChangeCallback traverses through all ports' status */
void GenericLinkStateISR()
{
    SetEvent(LinkStateHandler, LinkStateEvent);
    writel(CFG_SW_INTR_CLR, INTR_CLR_LINK_ALL);
}

/**
  * @api    GetOsVersion
  * @brief  Gets the version of OS
  * @param  os_version - returns the version of os
  * @retval None
  */

void GetOsVersion(char **osVersion)
{
   *osVersion = OS_IMAGE_VERSION;
}
