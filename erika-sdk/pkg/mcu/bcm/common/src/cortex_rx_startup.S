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

#include "cpu/cortex_rx/inc/ee_crash.h"
#include "mcu/bcm/common/inc/cortex_rx_mpu.h"

	.syntax unified
	.arch armv7-r
	.cpu cortex-r4

/*=====================================================
    Macros
=======================================================*/

#define EXCEPTION_MODE						\
	.code   32
#ifdef __BCM8953X__
    .equ F2_STATUS,0x0a800304      /* MISC_F2_Image_status - F2 Image Status*/
#endif
/*=====================================================
    Externs
=======================================================*/
	.extern __bss_start__
	.extern __bss_end__
	.extern __boot_stack__
	.extern __fiq_stack__
	.extern __exception_stack__
#ifdef __ENABLE_MPU__
	.extern mpu_config
#endif

/*=====================================================
    Vector table
=======================================================*/
	EXCEPTION_MODE
	.section  .isr_vector
	.global  __isr_vector

__isr_vector:
	b     cortex_rx_default_reset
    b     cortex_rx_undef_instr
    b     cortex_rx_sw_int
    b     cortex_rx_prefetch_abort
    b     cortex_rx_data_abort
    b     cortex_rx_rom_reentry
    b     cortex_rx_irq
    b     cortex_rx_fiq
	.size  __isr_vector, .-__isr_vector


/*=====================================================
    Reset vector
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_default_reset

cortex_rx_default_reset:

    /* fiq mode stack */
	mov   	r4,	#0xD1
    msr   	cpsr_c,r4
    ldr		sp, =__fiq_stack__

    /* abt mode stack */
    mov		r4,#0xD7
    msr		cpsr_c,r4
    ldr		sp, =__exception_stack__

    /* undef mode stack */
    mov		r4,#0xDB
    msr		cpsr_c,r4
    ldr		sp, =__exception_stack__

    /* system mode and suppress interupts */
    mrs     r4,cpsr
    bic     r4,r4,#0x1f
    orr     r4,r4,#0xdF
    msr     cpsr_c,r4

#ifdef ENABLE_SOFT_RESET_FLAG
    // Check the init flag for a reentry
    ldr     r4,soft_reset_flag
    cmp     r4,#0
    bne     cortex_rx_reset_reentry

    // Set the init flag for next time
    add     r4,r4,#1
    str     r4,soft_reset_flag
#endif

    /* little endian */
    SETEND LE

    /* clear RE+VE+NMFI+IC+C,
     * enable branch prediction,
     * vectors to low address,
     * branch predict clear */
    mrc     p15,0,r5,c1,c0,0
    ldr     r6,p15clr
    bic     r5,r5,r6
    orr     r5,r5,#0x0800
    bic     r5,r5,0x2000
    mcr     p15,0,r5,c1,c0,0

    /* Setup stack */
    ldr     sp, =__boot_stack__

#ifdef __ENABLE_MPU__
    bl  mpu_config
#endif

#ifdef __BCM8953X__
    ldr     r7, =F2_STATUS        /* r7 <-- FIRMWARE_STATUS address*/
    ldrh    r8, [r7]              /* r8 <-- content of register FIRMWARE_STATUS*/
    orr     r8, r8, #0x8000       /* set bit 15 (Firmware execution completed)*/
    strh    r8, [r7]              /* update register FIRMWARE_STATUS - F2*/
#endif

#ifdef CORTEX_RX_ICACHE
    mcr     p15,0,r4,c7,c5,0        // Invalidate icache
#endif

#ifdef CORTEX_RX_DCACHE
    mcr 	p15,0,r0,c15,c5,0		//Dcache invalidate
#endif
#ifdef CORTEX_RX_BPREDICT
    mcr 	p15,0,r0,c7,c5,6		// Branch predict clear
#endif
clear_bss:
	ldr     r5, =__bss_start__
	ldr     r6, =__bss_end__
	cmp     r6,r5
	beq     main_boot_call
	mov     r7,#0
1:	str     r7,[r5],#4
	cmp     r6,r5
	bhi     1b

main_boot_call:
	blx     main
#ifdef ENABLE_SOFT_RESET_FLAG
soft_reset_flag:       .word   0
#endif
p15clr:         .word   0x4b001004


/*=====================================================
    rom re-entry
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_rom_reentry

cortex_rx_rom_reentry:
	mov 	r0, #0
	cmp     r0,#0
	beq 	cortex_rx_rom_reentry


/*=====================================================
    irq
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_irq

cortex_rx_irq:
	sub 	lr, lr, #4

	srsdb 	sp!, #31
	/* Switch to system mode to save current execution state*/
	cpsid 	i, #31
	push 	{r0-r3, r12, lr}

	bl		EE_cortex_rx_irq_handler

	/* Restore the execution context from the current stack */
	pop 	{r0-r3, r12, lr}

	rfeia	sp!


/*=====================================================
    fiq
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_fiq

cortex_rx_fiq:
	/* Save current execution state*/
	push 	{r0-r3, r12, lr}

	bl		EE_cortex_rx_fiq_handler

	/* Restore the execution context from the current stack */
	pop 	{r0-r3, r12, lr}
	subs    pc,lr,#4


/*=====================================================
    unified exception handling
=======================================================*/
.macro cortex_rx_default_exception, reason
	cpsID   if

	sub		sp, sp, #EE_REG_FRAME_SIZE
	stmia 	sp, {r0-r12}
	add		r2, sp, #0x34

	/* PC of faulting code */
	mov     r6, lr
	/* CPSR of faulting code */
	mrs     r0, spsr
	mov 	r7, r0

	/* remember current mode, so we can re-enter */
	mrs     r1, cpsr

	/* enter mode that was in place at exception time
     * to save store SP_mode and LR_mode
	 */
	orr     r0,r0,#0xc0
	msr     cpsr_c,r0

	mov     r4, r13
	mov     r5, r14

	/* restore to exception mode */
	msr     cpsr_c, r1

	/* Push SP, LR, PC and CPSR of faulting code to stack */
	stmia	r2, {r4, r5, r6, r7}

	mov     r0, sp
    mov     r1, #\reason
	bl      EE_cortex_rx_exception_handler
.endm


/*=====================================================
    reset re-entry
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_reset_reentry
cortex_rx_reset_reentry:
	cortex_rx_default_exception EE_ROM_REENTRY


/*=====================================================
    undefined instruction
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_undef_instr
cortex_rx_undef_instr:
	cortex_rx_default_exception EE_UNDEF_INSTR


/*=====================================================
    software interrupt
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_sw_int
cortex_rx_sw_int:
	 cortex_rx_default_exception EE_SOFTWARE_INTR


/*=====================================================
    instruction prefetch abort
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_prefetch_abort
cortex_rx_prefetch_abort:
	 cortex_rx_default_exception EE_PREFETCH_ABORT


/*=====================================================
    data load abort
=======================================================*/
	EXCEPTION_MODE
	.global  cortex_rx_data_abort
cortex_rx_data_abort:
	 cortex_rx_default_exception EE_DATA_ABORT

