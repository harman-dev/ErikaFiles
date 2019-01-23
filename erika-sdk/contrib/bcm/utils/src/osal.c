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


/******************************************************************************
 File Name:  osal.c
 Descritpion: This file provides an abstraction for the OS calls
******************************************************************************/

#include "eecfg.h"
#include "ee_internal.h"
#include "bcm/utils/inc/osal.h"

static OsalStatusType osal_err(StatusType status) {
    OsalStatusType err;
    switch(status) {
        case E_OK:
            err = OSAL_ERR_OK;
            break;
        case E_OS_ACCESS:
        case E_OS_ID:
            err = OSAL_ERR_OS_INVALID_TASKID;
            break;
        case E_OS_CALLEVEL:
            err = OSAL_ERR_OS_INVALID_CONTEXT;
            break;
        case E_OS_LIMIT:
            err = OSAL_ERR_OS_CORE;
            break;
        case E_OS_NOFUNC:
            err = OSAL_ERR_OS_INVALID_OBJECT;
            break;
        case E_OS_RESOURCE:
            err = OSAL_ERR_OS_RESOURCE_BUSY;
            break;
        case E_OS_STATE:
            err = OSAL_ERR_OS_INVALID_STATE;
            break;
        case E_OS_VALUE:
            err = OSAL_ERR_OS_INVALID_PARAM;
            break;
        case E_OS_SYS_INIT:
            err = OSAL_ERR_OS_SYS_INIT;
            break;
        default:
            err = OSAL_ERR_UNKNOWN;
    }
    return err;
}

/******************************************************************************
 FUNCTION NAME: OsalGetEvent

 ARGUMENTS:
 taskId -> Id of the task whose mask has to be returned
 event -> returned Mask

 DESCRIPTION: Wrapper function for GetEvent function

 RETURNS: Status Error
******************************************************************************/

OsalStatusType OsalGetEvent(OsalTaskType taskId, OsalEventMaskRefType event)
{
    StatusType status;
    status = GetEvent(taskId, event);
    return osal_err(status);
}

/******************************************************************************
 FUNCTION NAME: OsalWaitEvent

 ARGUMENTS:
 eventMask -> Mask of the event to be waited upon

 DESCRIPTION: Wrapper function for WaitEvent function

 RETURNS: Status Error
******************************************************************************/

OsalStatusType OsalWaitEvent(OsalEventMaskType eventMask)
{
    StatusType status;
    status = WaitEvent(eventMask);
    return osal_err(status);
}


/******************************************************************************
 FUNCTION NAME: OsalClearEvent

 ARGUMENTS:
 eventMask -> Mask of the events to be cleared

 DESCRIPTION: Wrapper function for ClearEvent function

 RETURNS: Status Error
******************************************************************************/
OsalStatusType OsalClearEvent(OsalEventMaskType eventMask)
{
    StatusType status;
    status = ClearEvent(eventMask);
    return osal_err(status);
}


/******************************************************************************
 FUNCTION NAME: OsalSetEvent

 ARGUMENTS:
 taskId -> Id of the task
 eventMask -> Mask of the events to be set

 DESCRIPTION: Wrapper function for SetEvent function

 RETURNS: Status Error
******************************************************************************/
OsalStatusType OsalSetEvent(OsalTaskType taskId, OsalEventMaskType eventMask)
{
    StatusType status;
    status = SetEvent(taskId, eventMask);
    return osal_err(status);
}


/******************************************************************************
 FUNCTION NAME: OsalActivateTask

 ARGUMENTS:
 taskId -> Id of the task to be activated

 DESCRIPTION: Wrapper function for ActivateTask function

 RETURNS: Status Error
******************************************************************************/
OsalStatusType OsalActivateTask(OsalTaskType taskId)
{
    StatusType status;
    status = ActivateTask(taskId);
    return osal_err(status);
}


/******************************************************************************
 FUNCTION NAME: OsalGetResource

 ARGUMENTS:
 resourceId -> Id of the resource reqd

 DESCRIPTION: Wrapper function for GetResource function

 RETURNS: Status Error
******************************************************************************/
OsalStatusType OsalGetResource(OsalResourceType resourceId)
{
    StatusType status;
    status = GetResource(resourceId);
    return osal_err(status);
}

/******************************************************************************
 FUNCTION NAME: OsalReleaseResource

 ARGUMENTS:
 resourceId -> Id of the resource to be released

 DESCRIPTION: Wrapper function for ReleaseResource function

 RETURNS: Status Error
******************************************************************************/
OsalStatusType OsalReleaseResource(OsalResourceType resourceId)
{
    StatusType status;
    status = ReleaseResource(resourceId);
    return osal_err(status);
}
