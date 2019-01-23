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

#include "bcm/drivers/inc/uart.h"
#include "bcm/drivers/inc/uart_pl011.h"
#include "ee_os.h"

#ifdef STATS

#define INCR_STAT(stat, amt)         do { uart_stats.(stat) += (amt); } while (0)

struct {
	int32_t         rx_bytes;
	int32_t         tx_bytes;
	int32_t         interrupts;
	int32_t         errors;
	int32_t         rx_overruns;
} uart_stats;

static stats_entry_t uart_stats_array[] = {
    { "rx bytes", &uart_stats.rx_bytes, NULL },
    { "tx bytes", &uart_stats.tx_bytes, NULL },
    { "interrupts", &uart_stats.interrupts, NULL },
    { "errors", &uart_stats.errors, NULL },
    { "rx overruns", &uart_stats.rx_overruns, NULL },
};

static stats_group_t uart_group = {
    "UART",
    sizeof(uart_stats_array)/sizeof(uart_stats_array[0]),
    uart_stats_array
};

#else
#define INCR_STAT(stat, amt)            do { } while (0)
#endif

static uart_t uart;
static uint32_t uart_is_initialized;

uint32_t uart_initialized(void)
{
	return uart_is_initialized;
}

static void uart_config(void)
{
    /* Init pl011 data structure and H/W */
    uart.regs = (pl011_regs_t *)UART_BASE;
    uart.rx_head = uart.rx_tail = uart.tx_head = uart.tx_tail = 0;

    InitSem(&uart.SemTx, 0);
    InitSem(&uart.SemRx, 0);

    /* Disable UART since it could be enabled by bootrom. As per pl011 spec,
     * wait for uart to finish the last transmit before programming the
     * control registers. */
    UART_REGS(uart)->cr = UART_REGS(uart)->cr & (~(CR_EN));
    while (UART_REGS(uart)->fr & FR_BUSY) { }

    /*Clear all pending interrupts */
    UART_REGS(uart)->icr = UART_REGS(uart)->ris;


    /* Clear the FIFO by clearing the FIFO enable */
    UART_REGS(uart)->lcr_h = 0;

    /* RX 1/8; TX 1/8 fifo levels for interrupts */
    UART_REGS(uart)->ifls = IFLS_RX_1_8 | IFLS_TX_1_8;

#ifndef __FPGA__
    /* The divisors must be set before the lcr
     * writing the lcr triggers a load of all 3 */
    UART_REGS(uart)->ibrd = UART_DIVISOR;
    UART_REGS(uart)->fbrd = (UART_CLOCK - (UART_DIVISOR * 16 * UART_BAUD)) *
				64 / (16 * UART_BAUD);
#else
    /* ON FPGA Setup baud rate is fixed to 9600
     * and actual clock rate is ~0.153598MHz */
    UART_REGS(uart)->ibrd = 1UL;
    UART_REGS(uart)->fbrd = 0UL;

#endif

    /* The standard configuration is 8-N-no parity. */
    UART_REGS(uart)->lcr_h = LCR_WLEN8 | LCR_NOPAR |
        //  ((UART_STOP_BITS == 1) ? LCR_STOP1 : LCR_STOP2) | LCR_FEN;
        ((UART_STOP_BITS == 1) ? LCR_STOP1 : LCR_STOP2);

    /* Clear all interrupts */
    UART_REGS(uart)->icr = ICR_OEIS | ICR_BEIS | ICR_PEIS | ICR_FEIS |
            ICR_RTIS | ICR_TXIS | ICR_RXIS | ICR_DSRMIS | ICR_DCDMIS |
            ICR_CTSMIS | ICR_RIMIS;

    UART_REGS(uart)->imsc = IMSC_OEIM | IMSC_BEIM | IMSC_PEIM |
			IMSC_FEIM | IMSC_TXIM | IMSC_RXIM;

    if(is_printf_polled() > 0UL) {
        UART_REGS(uart)->imsc &= ~(IMSC_TXIM) ;
    }

    UART_REGS(uart)->cr = CR_CTSEN | CR_RTSEN | CR_TXE | CR_RXE | CR_EN;
}

ISR2(uart_irq_handler)
{
	uint32_t mis;

	mis = UART_REGS(uart)->mis;
	INCR_STAT(interrupts, 1L);

	if ((mis & MIS_TXIS) != 0UL) {
		/* Clear the interrupt */
		UART_REGS(uart)->icr = ICR_TXIS;

		while ((!UART_TX_EMPTY(uart)) &&
			   ((UART_REGS(uart)->fr & FR_TXFF) == 0UL)) {
			/* Fill the TX FIFO */
			INCR_STAT(tx_bytes, 1L);
			UART_REGS(uart)->dr = uart.tx_buffer[uart.tx_head];
			uart.tx_head = UART_TX_INCR(uart.tx_head);
		}
	}
	if ((mis & MIS_RXIS) != 0UL) {
		/* Clear the interrupt */
		UART_REGS(uart)->icr = ICR_RXIS;

		/* Drain the RX FIFO */
		while ((UART_REGS(uart)->fr & FR_RXFE) == 0UL) {
			uint32_t dr = UART_REGS(uart)->dr;
			if (UART_RX_FULL(uart)) {
				/* The buffer is full so we discard the byte */
				INCR_STAT(rx_overruns, 1L);
			} else {
				uart.rx_buffer[uart.rx_tail] = (uint8_t)(dr & 0xffUL);
				uart.rx_tail = UART_RX_INCR(uart.rx_tail);
			}
			INCR_STAT(rx_bytes, 1L);
			if ((dr & (DR_OE|DR_BE|DR_PE|DR_FE)) != 0UL) {
				INCR_STAT(errors, 1L);
			}
		}
	}
	/* Restart any thread waiting for input */
	if (!UART_RX_EMPTY(uart)) {
		PostSem(&uart.SemRx);
	}

	/* Clear the receive and transmit interrupts */
	if ((mis & MIS_OEIS) == MIS_OEIS) {
		UART_REGS(uart)->icr = ICR_OEIS;
		INCR_STAT(errors, 1L);
	}
	if ((mis & MIS_BEIS) == MIS_BEIS) {
		UART_REGS(uart)->icr = ICR_BEIS;
		INCR_STAT(errors, 1L);
	}
	if ((mis & MIS_PEIS) == MIS_PEIS) {
		UART_REGS(uart)->icr = ICR_PEIS;
		INCR_STAT(errors, 1L);
	}
	if ((mis & MIS_FEIS) == MIS_FEIS) {
		UART_REGS(uart)->icr = ICR_FEIS;
		INCR_STAT(errors, 1L);
	}
	if ((mis & MIS_RTIS) == MIS_RTIS) {
		UART_REGS(uart)->icr = ICR_RTIS;
		INCR_STAT(errors, 1L);
	}
}

void uart_init(void)
{
	uart_config();
    uart_is_initialized = 1UL;
}

int32_t uart_putc_polled(uint8_t ch)
{
	EE_UREG flags;

	flags = EE_hal_suspendIRQ();
	while ((UART_REGS(uart)->fr & FR_TXFF) == FR_TXFF) {}
	UART_REGS(uart)->dr = (uint32_t)ch;
	EE_hal_resumeIRQ(flags);

    return 0;
}

int32_t uart_putc (uint8_t c)
{
	int32_t rc = 0L;
	EE_UREG flags;

    flags = EE_hal_suspendIRQ();

    /* We can not block caller (by using semaphore
     * or other mechanism) even if SW TX Q is full
     * for two reasons:
     * 1) it can be called from ISR context
     * 2) We can not use resources in
     * ERIKA Error hook calls (platform test
     * implements error hooks and does a print)
     * So if UART TX is slow (low baud rate),
     * SW TX Q might become full and it will
     * overide the characters in the Q
     */
    uart.tx_buffer[uart.tx_tail] = c;
    uart.tx_tail = UART_TX_INCR(uart.tx_tail);

    /* HW TX FIFO available?
     * push out a character from head immediately
     */
    if ((UART_REGS(uart)->fr & FR_TXFF) == 0U) {
	    INCR_STAT(tx_bytes, 1);
	    UART_REGS(uart)->dr = (uint32_t)(uart.tx_buffer[uart.tx_head]);
	    uart.tx_head = UART_TX_INCR(uart.tx_head);
    }

    EE_hal_resumeIRQ(flags);

    return (rc);
}

int32_t uart_getc (uint8_t *c)
{
	int32_t rc = 0L;
	EE_UREG flags;

	flags = EE_hal_suspendIRQ();

	if (UART_RX_EMPTY(uart)) {
		/* Wait for the receive buffer to be filled */
		EE_hal_resumeIRQ(flags);
		WaitSem(&uart.SemRx);
		flags = EE_hal_suspendIRQ();
	}

	if (UART_RX_EMPTY(uart)) {
		rc = -1;
	} else {
		*c = uart.rx_buffer[uart.rx_head];
		uart.rx_head = UART_RX_INCR(uart.rx_head);
	}

	EE_hal_resumeIRQ(flags);

	return (rc);
}
