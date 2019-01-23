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
 File Name:  stack_status.c
 Description: Function(s) to aid analyse stack status
******************************************************************************/

#include <stdint.h>
#include "ee_internal.h"
#include <eecfg.h>
#include <bcm/utils/inc/stack_status.h>

extern struct Brcm_Stacks Brcm_system_tos[EE_CORTEX_RX_SYSTEM_TOS_SIZE];
static void stack_set(void *s, uint8_t tid, uint32_t sz, uint32_t fill_sz);

/* Add End-Of-Stack as HEADER and include Task-ID and size of stack */
static void stack_set(void *s, uint8_t tid, uint32_t sz, uint32_t fill_sz)
{
    uint32_t stack_fill_sz = fill_sz;
    struct eos_identifier *eos = (struct eos_identifier *)s;
    /* If there is enough space to add a marker */
    if (stack_fill_sz > sizeof(struct eos_identifier)
            && (s != NULL)) {
        eos->magic_s = EOS_MAGIC;
        eos->magic_e = EOS_MAGIC;
        eos->tid = tid;
        eos->sz = sz; /* This will be the actual size of stack */
#ifdef __ENABLE_STACK_FILL__
        /* Exclude the marker */
        stack_fill_sz = stack_fill_sz - sizeof(struct eos_identifier);
        memset((void *)eos->fill, STACK_FILL_PATTERN, stack_fill_sz);
#endif
    }
}

void StackStatusInit()
{
    uint8_t i;
    for (i = (uint8_t)0; i < (uint32_t)EE_CORTEX_RX_SYSTEM_TOS_SIZE; i++) {
        stack_set((void *)Brcm_system_tos[i].base,
            (i - (uint8_t)1), /* Since the first entry is of IRQ, (i - 1) is the tid */
            (uint32_t)(Brcm_system_tos[i].size << 2), /* Size is of int32 */
            (uint32_t)(Brcm_system_tos[i].fillSize << 2)); /* Size is of int32 */
    }
}

