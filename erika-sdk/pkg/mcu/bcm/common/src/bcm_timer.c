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
#include "bcm/utils/inc/bcm_delay.h"
#include "ee_internal.h"
#include "mcu/bcm/common/inc/bcm_timer.h"


typedef struct {
    uint32_t accumulated_error;
    uint32_t error_per_tick;
    uint32_t reload;
} timer_periodic_error_t;

static timer_periodic_error_t timer_periodic_error;

static uint64_t timer_ns_per_tick;
static volatile uint64_t timer_ticks_ms;

/* For periodic, add error correction when interrupt is raised */
static uint32_t get_error_ns()
{
	uint32_t error_ns = 0UL;

	if (timer_periodic_error.error_per_tick != 0UL) {
		timer_periodic_error.accumulated_error += timer_periodic_error.error_per_tick;

		/* Adjust the clock for the accumulated error, Use 1024 */
		error_ns = timer_periodic_error.accumulated_error >> 10UL;
		timer_periodic_error.accumulated_error =
				(timer_periodic_error.accumulated_error & 0x3ffUL) + (24UL * error_ns);
	}
	return error_ns;
}

static uint32_t ns_since_tick(TIMER_BASE *arg)
{

    return (timer_periodic_error.reload - timer_get_current_value(arg)) * ((uint32_t)NS_PER_TICK);
}

void timer_init(void)
{
	uint64_t time_ns = SYS_TICK_US * NS_PER_US;

    /* Override configured SYS_TICK_US if it is less than 1ms as minimum */
    if (time_ns < NS_PER_MS)
        time_ns = NS_PER_MS;

    uint32_t load = timer_load(time_ns);
    /* Error correction factors */
	timer_periodic_error.accumulated_error = 0UL;
	timer_periodic_error.error_per_tick = (uint32_t)((time_ns * PS_PER_NS) -
					(PS_PER_TICK * load));
	timer_periodic_error.reload = load;

    timer_ns_per_tick = timer_set_periodic(TIMER_PERIODIC, time_ns);
}

uint64_t get_time_ns(void)
{
    uint64_t time_ns, now;
    /* There is a race where a tick fires here.  So we check to make
       sure that tick_nanosecs has not been updated during our read of
       the sp804 remainder reg. */
    do {
        time_ns = timer_ticks_ms;
        now = time_ns + (uint64_t)ns_since_tick(TIMER_PERIODIC);
    } while (time_ns != timer_ticks_ms);
    return now;
}

uint64_t get_time_ms(void)
{
    uint64_t time_ms;
    EE_UREG flags;
    flags = EE_hal_suspendIRQ();
    time_ms = timer_ticks_ms;
    EE_hal_resumeIRQ(flags);
    return time_ms;
}

/* Periodic timer handler */
ISR2(timer0_irq_handler)
{
    timer_ticks_ms += timer_ns_per_tick;
    timer_clear_interrupt(TIMER_PERIODIC);
    timer_ticks_ms += get_error_ns();

#if !defined(__OO_NO_ALARMS__) && defined(SystemTimer)
    IncrementCounterHardware(SystemTimer);
#endif
}

/* One shot timer handler */
ISR2(timer1_irq_handler)
{
    timer_clear_interrupt(TIMER_ONESHOT);
    timerq_process(get_time_ns());
}
