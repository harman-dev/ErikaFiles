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

#include "bcm/drivers/inc/vic.h"
#include "ee_internal.h"
#define DEFAULT_IRQ_HANDLER  vic_default_irq_handler

typedef void (*irq_handler_t)(void);

static irq_handler_t irq_handlers[VIC_IRQ_COUNT];
volatile vic_t *vic[VIC_COUNT];

/* Generic irq Handler */
void irq_generic_handler(void)
{
	EE_UINT32 irq = VIC_GET_ACT_IRQ_ADDR;

	(*irq_handlers[irq])();
	VIC_CLEAR_ACT_IRQ_ADDR(irq);
}

/* Default irq Handler */
static void vic_default_irq_handler(void)
{
	EE_UINT32 irq = VIC_GET_ACT_IRQ_ADDR;

	LOG_ERROR("Undefined irq(%u)\n", irq);
	VIC_CLEAR_ACT_IRQ_ADDR(irq);
}

static void vic_irq_init(void)
{
#ifdef	VIC_INT_00_ISR
	extern void VIC_INT_00_ISR(void);
	irq_handlers[VIC_INT_00_NUM] = VIC_INT_00_ISR;
#else
	irq_handlers[VIC_INT_00_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_01_ISR
	extern void VIC_INT_01_ISR(void);
	irq_handlers[VIC_INT_01_NUM] = VIC_INT_01_ISR;
#else
	irq_handlers[VIC_INT_01_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_02_ISR
	extern void VIC_INT_02_ISR(void);
	irq_handlers[VIC_INT_02_NUM] = VIC_INT_02_ISR;
#else
	irq_handlers[VIC_INT_02_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_03_ISR
	extern void VIC_INT_03_ISR(void);
	irq_handlers[VIC_INT_03_NUM] = VIC_INT_03_ISR;
#else
	irq_handlers[VIC_INT_03_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_04_ISR
	extern void VIC_INT_04_ISR(void);
	irq_handlers[VIC_INT_04_NUM] = VIC_INT_04_ISR;
#else
	irq_handlers[VIC_INT_04_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_05_ISR
	extern void VIC_INT_05_ISR(void);
	irq_handlers[VIC_INT_05_NUM] = VIC_INT_05_ISR;
#else
	irq_handlers[VIC_INT_05_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_06_ISR
	extern void VIC_INT_06_ISR(void);
	irq_handlers[VIC_INT_06_NUM] = VIC_INT_06_ISR;
#else
	irq_handlers[VIC_INT_06_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_07_ISR
	extern void VIC_INT_07_ISR(void);
	irq_handlers[VIC_INT_07_NUM] = VIC_INT_07_ISR;
#else
	irq_handlers[VIC_INT_07_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_08_ISR
	extern void VIC_INT_08_ISR(void);
	irq_handlers[VIC_INT_08_NUM] = VIC_INT_08_ISR;
#else
	irq_handlers[VIC_INT_08_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_09_ISR
	extern void VIC_INT_09_ISR(void);
	irq_handlers[VIC_INT_09_NUM] = VIC_INT_09_ISR;
#else
	irq_handlers[VIC_INT_09_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_0A_ISR
	extern void VIC_INT_0A_ISR(void);
	irq_handlers[VIC_INT_0A_NUM] = VIC_INT_0A_ISR;
#else
	irq_handlers[VIC_INT_0A_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_0B_ISR
	extern void VIC_INT_0B_ISR(void);
	irq_handlers[VIC_INT_0B_NUM] = VIC_INT_0B_ISR;
#else
	irq_handlers[VIC_INT_0B_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_0C_ISR
	extern void VIC_INT_0C_ISR(void);
	irq_handlers[VIC_INT_0C_NUM] = VIC_INT_0C_ISR;
#else
	irq_handlers[VIC_INT_0C_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_0D_ISR
	extern void VIC_INT_0D_ISR(void);
	irq_handlers[VIC_INT_0D_NUM] = VIC_INT_0D_ISR;
#else
	irq_handlers[VIC_INT_0D_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_0E_ISR
	extern void VIC_INT_0E_ISR(void);
	irq_handlers[VIC_INT_0E_NUM] = VIC_INT_0E_ISR;
#else
	irq_handlers[VIC_INT_0E_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_0F_ISR
	extern void VIC_INT_0F_ISR(void);
	irq_handlers[VIC_INT_0F_NUM] = VIC_INT_0F_ISR;
#else
	irq_handlers[VIC_INT_0F_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_10_ISR
	extern void VIC_INT_10_ISR(void);
	irq_handlers[VIC_INT_10_NUM] = VIC_INT_10_ISR;
#else
	irq_handlers[VIC_INT_10_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_11_ISR
	extern void VIC_INT_11_ISR(void);
	irq_handlers[VIC_INT_11_NUM] = VIC_INT_11_ISR;
#else
	irq_handlers[VIC_INT_11_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_12_ISR
	extern void VIC_INT_12_ISR(void);
	irq_handlers[VIC_INT_12_NUM] = VIC_INT_12_ISR;
#else
	irq_handlers[VIC_INT_12_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_13_ISR
	extern void VIC_INT_13_ISR(void);
	irq_handlers[VIC_INT_13_NUM] = VIC_INT_13_ISR;
#else
	irq_handlers[VIC_INT_13_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_14_ISR
	extern void VIC_INT_14_ISR(void);
	irq_handlers[VIC_INT_14_NUM] = VIC_INT_14_ISR;
#else
	irq_handlers[VIC_INT_14_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_15_ISR
	extern void VIC_INT_15_ISR(void);
	irq_handlers[VIC_INT_15_NUM] = VIC_INT_15_ISR;
#else
	irq_handlers[VIC_INT_15_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_16_ISR
	extern void VIC_INT_16_ISR(void);
	irq_handlers[VIC_INT_16_NUM] = VIC_INT_16_ISR;
#else
	irq_handlers[VIC_INT_16_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_17_ISR
	extern void VIC_INT_17_ISR(void);
	irq_handlers[VIC_INT_17_NUM] = VIC_INT_17_ISR;
#else
	irq_handlers[VIC_INT_17_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_18_ISR
	extern void VIC_INT_18_ISR(void);
	irq_handlers[VIC_INT_18_NUM] = VIC_INT_18_ISR;
#else
	irq_handlers[VIC_INT_18_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_19_ISR
	extern void VIC_INT_19_ISR(void);
	irq_handlers[VIC_INT_19_NUM] = VIC_INT_19_ISR;
#else
	irq_handlers[VIC_INT_19_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_1A_ISR
	extern void VIC_INT_1A_ISR(void);
	irq_handlers[VIC_INT_1A_NUM] = VIC_INT_1A_ISR;
#else
	irq_handlers[VIC_INT_1A_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_1B_ISR
	extern void VIC_INT_1B_ISR(void);
	irq_handlers[VIC_INT_1B_NUM] = VIC_INT_1B_ISR;
#else
	irq_handlers[VIC_INT_1B_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_1C_ISR
	extern void VIC_INT_1C_ISR(void);
	irq_handlers[VIC_INT_1C_NUM] = VIC_INT_1C_ISR;
#else
	irq_handlers[VIC_INT_1C_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_1D_ISR
	extern void VIC_INT_1D_ISR(void);
	irq_handlers[VIC_INT_1D_NUM] = VIC_INT_1D_ISR;
#else
	irq_handlers[VIC_INT_1D_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_1E_ISR
	extern void VIC_INT_1E_ISR(void);
	irq_handlers[VIC_INT_1E_NUM] = VIC_INT_1E_ISR;
#else
	irq_handlers[VIC_INT_1E_NUM] = DEFAULT_IRQ_HANDLER;
#endif

#ifdef	VIC_INT_1F_ISR
	extern void VIC_INT_1F_ISR(void);
	irq_handlers[VIC_INT_1F_NUM] = VIC_INT_1F_ISR;
#else
	irq_handlers[VIC_INT_1F_NUM] = DEFAULT_IRQ_HANDLER;
#endif
}

static void vic_config(void)
{
	register EE_UREG flags = EE_hal_suspendIRQ();

#ifdef	VIC_INT_00_ISR
#ifdef	VIC_INT_00_ISR_PRI
	VIC_SET_PRI(VIC_INT_00_NUM, VIC_INT_00_ISR_PRI);
#endif	/* VIC_INT_00_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_00_NUM);
#endif	/* VIC_INT_00_ISR */

#ifdef	VIC_INT_01_ISR
#ifdef	VIC_INT_01_ISR_PRI
	VIC_SET_PRI(VIC_INT_01_NUM, VIC_INT_01_ISR_PRI);
#endif	/* VIC_INT_01_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_01_NUM);
#endif	/* VIC_INT_01_ISR */

#ifdef	VIC_INT_02_ISR
#ifdef	VIC_INT_02_ISR_PRI
	VIC_SET_PRI(VIC_INT_02_NUM, VIC_INT_02_ISR_PRI);
#endif	/* VIC_INT_02_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_02_NUM);
#endif	/* VIC_INT_02_ISR */

#ifdef	VIC_INT_03_ISR
#ifdef	VIC_INT_03_ISR_PRI
	VIC_SET_PRI(VIC_INT_03_NUM, VIC_INT_03_ISR_PRI);
#endif	/* VIC_INT_03_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_03_NUM);
#endif	/* VIC_INT_03_ISR */

#ifdef	VIC_INT_04_ISR
#ifdef	VIC_INT_04_ISR_PRI
	VIC_SET_PRI(VIC_INT_04_NUM, VIC_INT_04_ISR_PRI);
#endif	/* VIC_INT_04_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_04_NUM);
#endif	/* VIC_INT_04_ISR */

#ifdef	VIC_INT_05_ISR
#ifdef	VIC_INT_05_ISR_PRI
	VIC_SET_PRI(VIC_INT_05_NUM, VIC_INT_05_ISR_PRI);
#endif	/* VIC_INT_05_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_05_NUM);
#endif	/* VIC_INT_05_ISR */

#ifdef	VIC_INT_06_ISR
#ifdef	VIC_INT_06_ISR_PRI
	VIC_SET_PRI(VIC_INT_06_NUM, VIC_INT_06_ISR_PRI);
#endif	/* VIC_INT_06_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_06_NUM);
#endif	/* VIC_INT_06_ISR */

#ifdef	VIC_INT_07_ISR
#ifdef	VIC_INT_07_ISR_PRI
	VIC_SET_PRI(VIC_INT_07_NUM, VIC_INT_07_ISR_PRI);
#endif	/* VIC_INT_07_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_07_NUM);
#endif	/* VIC_INT_07_ISR */

#ifdef	VIC_INT_08_ISR
#ifdef	VIC_INT_08_ISR_PRI
	VIC_SET_PRI(VIC_INT_08_NUM, VIC_INT_08_ISR_PRI);
#endif	/* VIC_INT_08_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_08_NUM);
#endif	/* VIC_INT_08_ISR */

#ifdef	VIC_INT_09_ISR
#ifdef	VIC_INT_09_ISR_PRI
	VIC_SET_PRI(VIC_INT_09_NUM, VIC_INT_09_ISR_PRI);
#endif	/* VIC_INT_09_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_09_NUM);
#endif	/* VIC_INT_09_ISR */

#ifdef	VIC_INT_0A_ISR
#ifdef	VIC_INT_0A_ISR_PRI
	VIC_SET_PRI(VIC_INT_0A_NUM, VIC_INT_0A_ISR_PRI);
#endif	/* VIC_INT_0A_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_0A_NUM);
#endif	/* VIC_INT_0A_ISR */

#ifdef	VIC_INT_0B_ISR
#ifdef	VIC_INT_0B_ISR_PRI
	VIC_SET_PRI(VIC_INT_0B_NUM, VIC_INT_0B_ISR_PRI);
#endif	/* VIC_INT_0B_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_0B_NUM);
#endif	/* VIC_INT_0B_ISR */

#ifdef	VIC_INT_0C_ISR
#ifdef	VIC_INT_0C_ISR_PRI
	VIC_SET_PRI(VIC_INT_0C_NUM, VIC_INT_0C_ISR_PRI);
#endif	/* VIC_INT_0C_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_0C_NUM);
#endif	/* VIC_INT_0C_ISR */

#ifdef	VIC_INT_0D_ISR
#ifdef	VIC_INT_0D_ISR_PRI
	VIC_SET_PRI(VIC_INT_0D_NUM, VIC_INT_0D_ISR_PRI);
#endif	/* VIC_INT_0D_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_0D_NUM);
#endif	/* VIC_INT_0D_ISR */

#ifdef	VIC_INT_0E_ISR
#ifdef	VIC_INT_0E_ISR_PRI
	VIC_SET_PRI(VIC_INT_0E_NUM, VIC_INT_0E_ISR_PRI);
#endif	/* VIC_INT_0E_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_0E_NUM);
#endif	/* VIC_INT_0E_ISR */

#ifdef	VIC_INT_0F_ISR
#ifdef	VIC_INT_0F_ISR_PRI
	VIC_SET_PRI(VIC_INT_0F_NUM, VIC_INT_0F_ISR_PRI);
#endif	/* VIC_INT_0F_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_0F_NUM);
#endif	/* VIC_INT_0F_ISR */

#ifdef	VIC_INT_10_ISR
#ifdef	VIC_INT_10_ISR_PRI
	VIC_SET_PRI(VIC_INT_10_NUM, VIC_INT_10_ISR_PRI);
#endif	/* VIC_INT_10_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_10_NUM);
#endif	/* VIC_INT_10_ISR */

#ifdef	VIC_INT_11_ISR
#ifdef	VIC_INT_11_ISR_PRI
	VIC_SET_PRI(VIC_INT_11_NUM, VIC_INT_11_ISR_PRI);
#endif	/* VIC_INT_11_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_11_NUM);
#endif	/* VIC_INT_11_ISR */

#ifdef	VIC_INT_12_ISR
#ifdef	VIC_INT_12_ISR_PRI
	VIC_SET_PRI(VIC_INT_12_NUM, VIC_INT_12_ISR_PRI);
#endif	/* VIC_INT_12_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_12_NUM);
#endif	/* VIC_INT_12_ISR */

#ifdef	VIC_INT_13_ISR
#ifdef	VIC_INT_13_ISR_PRI
	VIC_SET_PRI(VIC_INT_13_NUM, VIC_INT_13_ISR_PRI);
#endif	/* VIC_INT_13_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_13_NUM);
#endif	/* VIC_INT_13_ISR */

#ifdef	VIC_INT_14_ISR
#ifdef	VIC_INT_14_ISR_PRI
	VIC_SET_PRI(VIC_INT_14_NUM, VIC_INT_14_ISR_PRI);
#endif	/* VIC_INT_14_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_14_NUM);
#endif	/* VIC_INT_14_ISR */

#ifdef	VIC_INT_15_ISR
#ifdef	VIC_INT_15_ISR_PRI
	VIC_SET_PRI(VIC_INT_15_NUM, VIC_INT_15_ISR_PRI);
#endif	/* VIC_INT_15_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_15_NUM);
#endif	/* VIC_INT_15_ISR */

#ifdef	VIC_INT_16_ISR
#ifdef	VIC_INT_16_ISR_PRI
	VIC_SET_PRI(VIC_INT_16_NUM, VIC_INT_16_ISR_PRI);
#endif	/* VIC_INT_16_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_16_NUM);
#endif	/* VIC_INT_16_ISR */

#ifdef	VIC_INT_17_ISR
#ifdef	VIC_INT_17_ISR_PRI
	VIC_SET_PRI(VIC_INT_17_NUM, VIC_INT_17_ISR_PRI);
#endif	/* VIC_INT_17_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_17_NUM);
#endif	/* VIC_INT_17_ISR */

#ifdef	VIC_INT_18_ISR
#ifdef	VIC_INT_18_ISR_PRI
	VIC_SET_PRI(VIC_INT_18_NUM, VIC_INT_18_ISR_PRI);
#endif	/* VIC_INT_18_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_18_NUM);
#endif	/* VIC_INT_18_ISR */

#ifdef	VIC_INT_19_ISR
#ifdef	VIC_INT_19_ISR_PRI
	VIC_SET_PRI(VIC_INT_19_NUM, VIC_INT_19_ISR_PRI);
#endif	/* VIC_INT_19_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_19_NUM);
#endif	/* VIC_INT_19_ISR */

#ifdef	VIC_INT_1A_ISR
#ifdef	VIC_INT_1A_ISR_PRI
	VIC_SET_PRI(VIC_INT_1A_NUM, VIC_INT_1A_ISR_PRI);
#endif	/* VIC_INT_1A_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_1A_NUM);
#endif	/* VIC_INT_1A_ISR */

#ifdef	VIC_INT_1B_ISR
#ifdef	VIC_INT_1B_ISR_PRI
	VIC_SET_PRI(VIC_INT_1B_NUM, VIC_INT_1B_ISR_PRI);
#endif	/* VIC_INT_1B_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_1B_NUM);
#endif	/* VIC_INT_1B_ISR */

#ifdef	VIC_INT_1C_ISR
#ifdef	VIC_INT_1C_ISR_PRI
	VIC_SET_PRI(VIC_INT_1C_NUM, VIC_INT_1C_ISR_PRI);
#endif	/* VIC_INT_1C_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_1C_NUM);
#endif	/* VIC_INT_1C_ISR */

#ifdef	VIC_INT_1D_ISR
#ifdef	VIC_INT_1D_ISR_PRI
	VIC_SET_PRI(VIC_INT_1D_NUM, VIC_INT_1D_ISR_PRI);
#endif	/* VIC_INT_1D_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_1D_NUM);
#endif	/* VIC_INT_1D_ISR */

#ifdef	VIC_INT_1E_ISR
#ifdef	VIC_INT_1E_ISR_PRI
	VIC_SET_PRI(VIC_INT_1E_NUM, VIC_INT_1E_ISR_PRI);
#endif	/* VIC_INT_1E_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_1E_NUM);
#endif	/* VIC_INT_1E_ISR */

#ifdef	VIC_INT_1F_ISR
#ifdef	VIC_INT_1F_ISR_PRI
	VIC_SET_PRI(VIC_INT_1F_NUM, VIC_INT_1F_ISR_PRI);
#endif	/* VIC_INT_1F_ISR_PRI */
	VIC_INT_ENABLE(VIC_INT_1F_NUM);
#endif	/* VIC_INT_1F_ISR */

	EE_hal_resumeIRQ(flags);
}

void irq_init(void)
{
	uint32_t i, j;

	for (i = 0UL; i < VIC_COUNT; i++) {
        vic[i] = (vic_t *)VIC_BASE(i);
        /* Clear anything pending */
        vic[i]->int_enable_clear =
                        (uint32_t)VIC0_VICINTENCLEAR_INTENABLECLEAR_MASK;
        /* coverity[self_assign] - assignment here clears pending interrupts */
        vic[i]->address = vic[i]->address;
        vic[i]->int_select = 0UL;
        vic[i]->soft_priority_mask =
                        (uint32_t)VIC0_VICSWPRIORITYMASK_SWPRIORITYMASK_MASK;

        /* Set the int vect address to interrupt number */
		for (j = 0UL; j < VIC_IRQ_COUNT; j++) {
			vic[VIC_NUM(i)]->vect_addr[VIC_IRQ_NUM(j)] = j;
		}
	}

	vic_irq_init();
    vic_config();
}
