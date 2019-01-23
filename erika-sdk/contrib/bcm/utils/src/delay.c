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

#include "bcm/utils/inc/list.h"
#include "bcm/utils/inc/bcm_delay.h"
#include "ee.h"

#define INIT_TIMERQ_ENTRY(new, time_ns)   \
    ({   \
        list_init(&((new).list));   \
        (new).timeout = (time_ns);    \
        (new).taskid = EE_stk_queryfirst();  \
    })

typedef struct sleep_entry {
    int32_t taskid;
    uint64_t timeout;
    list_t list;
}sleep_t;

static LIST_HEAD(timerq);

static void timerq_add(sleep_t *new)
{
    list_t *next = timerq.next;

    if (list_is_empty(&timerq) == 1L) {
        list_add(&timerq, &new->list);
        return;
    }

    while (next != &timerq) {
        sleep_t *sentry = LIST_ENTRY(next, sleep_t, list);
        if (new->timeout < sentry->timeout) {
            list_add(next, &new->list);
            return;
        }
        next = next->next;
    }
    list_add_tail(next->prev, &new->list);
}

static void nsleep(uint64_t time_ns)
{
    uint32_t flags;
    EventMaskType mask = 0UL;
    sleep_t new;

    INIT_TIMERQ_ENTRY(new, get_time_ns() + time_ns);

    flags = EE_hal_suspendIRQ();

    timerq_add(&new);
    if (list_is_first(&timerq, &new.list) == 1L)
        timer_set_oneshot(TIMER_ONESHOT, time_ns);

	EE_hal_resumeIRQ(flags);

    WaitEvent(SleepEvent);

    GetEvent(new.taskid, &mask);
    if ((mask & SleepEvent) == SleepEvent)
        ClearEvent(SleepEvent);
}

void sleep(uint32_t time_sec)
{
    nsleep(time_sec * NS_PER_SEC);
}

void msleep(uint32_t time_msec)
{
    nsleep(time_msec * NS_PER_MS);
}

void usleep(uint32_t time_usec)
{
    nsleep(time_usec * NS_PER_US);
}

void timerq_process(uint64_t now)
{
    while(!list_is_empty(&timerq)) {
        list_t *next = timerq.next;
        sleep_t *sentry = LIST_ENTRY(next, sleep_t, list);

        if (sentry->timeout > now) {
            timer_set_oneshot(TIMER_ONESHOT, (sentry->timeout - now));
            break;
        }

        SetEvent(sentry->taskid, SleepEvent);
        list_del(next);
    };
}

void ndelay(uint64_t time_ns)
{
    uint64_t wait_ns = get_time_ns() + time_ns;

    do {
        ;
    } while (wait_ns > get_time_ns());
}

void udelay(uint64_t time_us)
{
    ndelay(time_us * NS_PER_US);
}
