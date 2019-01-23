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

#include <string.h>
#include <stdint.h>
#include "bcm/drivers/inc/qspi.h"
#include "bcm/drivers/inc/gpio.h"
#include "ee_internal.h"

/* QSPI generic macros */
#define qspi_rev_id()		\
	reg_read32(QSPI_BSPI_REGISTERS_REVISION_ID)

#define DUMMY_DATA				(0x0)

#ifdef QSPI_DBG
#define qspi_trace(...)		LOG_DEBUG(__VA_ARGS__)
#else
#define qspi_trace(...)
#endif

/*
 * 0x28(0d40) cycles = 8 cycles (command) + 24 cycles (address) +
 * 			8 cycles (dummy byte)
 */
#define QSPI_CRC_IDLE_CYCLE_COUNT_NO_IGNORE	(0x28UL)

/*
 * Bits to words conversion - divide by 32 (8 * 4)
 */
#define BITS2WORDS(x)	((x) >> 5)

/*
 * Prefetch buffer size is 32 bytes = 8 words.
 */
#define PREFETCH_BUFF_SZ_IN_WORDS	(8UL)

/* Global variables */
/* Flag for qspi_init done */
static int qspi_init_done = 0L;
volatile int raf_num_words = 0L;

static void qspi_config_gpio(void)
{
	gpio_flash_mux_sel(FLASH_CS_FUNC_CS);

	gpio_flash_cs_oe_en(1);
	gpio_flash_cs_dout_en(1);
}

#ifdef QSPI_DBG
int mspi_set_clk(uint32_t freq)
{
	uint32_t div;
	uint32_t hclk_freq = dmu_get_hclk_freq();

	if(freq == 0)
		return ERR_INVAL;

	div = hclk_freq / freq;
	div /= 2;

	if((div < 8) || (div > 255))
		return ERR_INVAL;

	div &= (uint32_t)QSPI_MSPI_SPCR0_LSB_MSPI_SPCR0_LSB_SPBR_MASK;
	reg_write32((uint32_t)QSPI_MSPI_SPCR0_LSB, div);

	return ERR_OK;
}

int mspi_get_clk(uint32_t *freq)
{
	uint32_t hclk = dmu_get_hclk_freq();
	uint32_t reg = reg_read32((uint32_t)QSPI_MSPI_SPCR0_LSB);

	if(freq == NULL)
		return ERR_INVAL;

	reg &= (uint32_t)QSPI_MSPI_SPCR0_LSB_MSPI_SPCR0_LSB_SPBR_MASK;

	qspi_trace("%s:hclk:%u, spbr:0x%x\n", __func__, hclk, reg);

	*freq = hclk / (2 * reg);

	return ERR_OK;
}
#endif

__INLINE__ void mspi_set_qps(mspi_qp start_qp, mspi_qp end_qp)
{
	reg_write32((uint32_t)QSPI_MSPI_NEWQP, start_qp);
	reg_write32((uint32_t)QSPI_MSPI_ENDQP, end_qp);
}

#ifdef QSPI_DBG
int bspi_set_clk(uint32_t freq)
{
	dmu_qclk_src_t div_value;
	uint32_t divider;
	uint32_t ret_freq;
	uint32_t cpu_freq = dmu_get_cpuclk_freq();

	if (freq == 0)
		return ERR_INVAL;

	divider = cpu_freq / freq;

	switch (divider) {
	case 3:
		div_value = DMU_QCLK_SRC_CPUCLK_DIV_3;
		break;
	case 4:
		div_value = DMU_QCLK_SRC_CPUCLK_DIV_4;
		break;
	case 5:
		div_value = DMU_QCLK_SRC_CPUCLK_DIV_5;
		break;
	case 10:
		div_value = DMU_QCLK_SRC_CPUCLK_DIV_10;
		break;
	default:
		LOG_ERROR("%s: Invalid divider(%u) based on the frequency(%u)"
			" chosen\n", __func__, divider, freq);
		return ERR_INVAL;
	}
	ret_freq = dmu_set_qclk_freq(div_value);
	if(ret_freq != freq) {
		LOG_ERROR("%s: Invalid frequency set for qclk\n", __func__);
		return ERR_INVAL;
	}

	return ERR_OK;
}

int bspi_get_clk(uint32_t *freq)
{	if(freq == NULL)
		return ERR_INVAL;

	*freq = dmu_get_qclk_freq();

	return ERR_OK;
}
#endif

void mspi_start(void)
{
	uint32_t reg;

	mspi_wr_lock_en();
	mspi_spcr_cs_cont();

	reg = reg_read32((uint32_t)QSPI_MSPI_SPCR2);
	reg |= (uint32_t)QSPI_MSPI_SPCR2_MSPI_SPCR2_SPE_MASK;
	reg_write32((uint32_t)QSPI_MSPI_SPCR2, reg);
}

int32_t mspi_wait_trans_cmplt(void)
{
    uint32_t reg;
    uint32_t i = 0UL;
    int32_t err = ERR_OK;

    do {
        reg = reg_read32((uint32_t)QSPI_MSPI_MSPI_STATUS);
        if ((reg & (uint32_t)QSPI_MSPI_MSPI_STATUS_MSPI_MSPI_STATUS_SPIF_MASK) != 0UL)
            break;

        udelay(MSPI_STATUS_UDELAY);
        i++;
    } while (i < MSPI_STATUS_SPIF_TIMOUT_CNT);

    /* Polling timeout */
    if(i == MSPI_STATUS_SPIF_TIMOUT_CNT)
        err = ERR_TIMEOUT;
    else {
        reg &= ~(uint32_t)QSPI_MSPI_MSPI_STATUS_MSPI_MSPI_STATUS_SPIF_MASK;
        /* clear the SPIF bit */
        reg_write32((uint32_t)QSPI_MSPI_MSPI_STATUS, reg);
    }

    return err;
}

/* MSPI halting sequence:
 * -> Assert HALT bit in SPCR2 register
 * -> wait for HALTA bit to be set
 * -> clear HALTA bit
 * -> disable the MSPI by setting SPE bit
 */
void mspi_halt(void)
{
	uint32_t reg;

	reg = reg_read32((uint32_t)QSPI_MSPI_SPCR2);
	reg |= (uint32_t)QSPI_MSPI_SPCR2_MSPI_SPCR2_HALT_MASK;

	reg_write32((uint32_t)QSPI_MSPI_SPCR2, reg);

	/* wait for HALT bit to be set */
	do {
		reg = reg_read32((uint32_t)QSPI_MSPI_MSPI_STATUS);
	} while ((reg & ((uint32_t)QSPI_MSPI_MSPI_STATUS_MSPI_MSPI_STATUS_HALTA_MASK)) == 0UL);

	/* clear HALTA bit */
	reg &= ~((uint32_t)QSPI_MSPI_MSPI_STATUS_MSPI_MSPI_STATUS_HALTA_MASK);
	reg_write32((uint32_t)QSPI_MSPI_MSPI_STATUS, reg);

	/* Now its safe to disable the MSPI */
	reg = reg_read32((uint32_t)QSPI_MSPI_SPCR2);
	reg &= ~((uint32_t)QSPI_MSPI_SPCR2_MSPI_SPCR2_SPE_MASK);
	reg_write32((uint32_t)QSPI_MSPI_SPCR2, reg);
}

int mspi_xfer(char const *tx_data, char *rx_data, uint32_t len, int cont)
{
	uint32_t i;
    int ret = ERR_OK;

	if ((tx_data == NULL) || (len == 0)) {
		ret = ERR_INVAL;
        goto EXIT;
    }

	if (qspi_init_done == 0L) {
		ret = ERR_NOINIT;
        goto EXIT;
    }

	mspi_cdram cdram = MSPI_CDRAM_00;
	mspi_txram txram = MSPI_TXRAM_00;
	mspi_rxram rxram = MSPI_RXRAM_01;

	mspi_enable();

	/* Fill the TX FIFO and cdram registers*/
	for (i = 0UL; i < len; i++) {
		mspi_write_txram(tx_data[i], txram);
		if (i == (len - 1UL)) {
			if (cont != 0L) {
				mspi_cdram_cmd_continue(cdram);
			} else {
				mspi_cdram_cmd_stop(cdram);
			}
		} else {
			mspi_cdram_cmd_continue(cdram);
		}
		txram += 2UL;
		cdram++;
	}

	/* Program the Start and end QPs */
	mspi_set_qps(MSPI_QP_00, cdram - 1);

	mspi_start();
	ret = mspi_wait_trans_cmplt();
	if (ret != ERR_OK) {
		goto EXIT;
    }

	/* read the RX FIFO */
	for (i = 0UL; (i < len) && rx_data; i++) {
		rx_data[i] = mspi_read_rxram(rxram);
		rxram += 2;
	}

	if (cont == 0L)
		mspi_disable();

EXIT:
	return ret;

}

#ifdef __ENABLE_FLASH_QSPI_READ_BSPI_RAF__
/* Start data read using RAF
 */
static inline int32_t raf_start()
{
    uint32_t i = RAF_START_TIMEOUT;
    int32_t ret = ERR_OK;

    reg_write32(QSPI_RAF_CTRL, QSPI_RAF_CTRL_RAF_CTRL_START_MASK);
    while ((reg_read32(QSPI_RAF_CTRL) & (uint32_t)QSPI_RAF_CTRL_RAF_CTRL_START_MASK) != 0UL) {
        if(0UL == i) {
            break;
        }
        i--;
    }

    if(0UL == i) {
        qspi_trace("raf_start: raf start bit timeout\n");
        ret = ERR_TIMEOUT;
    }

    return ret;
}

/* Stop data read using RAF
 */
static inline int32_t raf_stop()
{
    uint32_t temp = reg_read32(QSPI_RAF_CTRL);
    uint32_t i = RAF_STOP_TIMEOUT;
    int32_t ret = ERR_OK;

    temp &= ~((uint32_t)QSPI_RAF_CTRL_RAF_CTRL_START_MASK);
    temp |= ((uint32_t)QSPI_RAF_CTRL_RAF_CTRL_CLEAR_MASK);
    reg_write32(QSPI_RAF_CTRL, temp);
    while ((reg_read32(QSPI_RAF_CTRL) & (uint32_t)QSPI_RAF_CTRL_RAF_CTRL_CLEAR_MASK) != 0UL) {
        if(0UL == i) {
            break;
        }
        i--;
    }

    if(0UL == i) {
        qspi_trace("raf_stop: raf_stop bit timeout\n");
        ret = ERR_TIMEOUT;
    }

    return ret;
}

static int raf_read_align_addr(uint32_t addr, char * const data, uint32_t len)
{
	uint32_t fifo_data, i;
    uint32_t status, fifo_empty, busy;
	uint32_t count = 0UL;
	uint32_t words = 0UL;
	uint32_t remain = len;
	uint32_t j = 1000UL;
    int32_t ret = ERR_OK;

	qspi_trace("raf_read: addr: 0x%x len: %d\n", addr, len);

	if ((data == NULL) || (len == 0UL)) {
		ret = ERR_INVAL;
        goto EXIT;
    }

	if (qspi_init_done == 0L) {
		ret = ERR_NOINIT;
        goto EXIT;
    }

	raf_set_addr(addr);

	/* if len is not multiple of 4 (word len of RAF)
	 * we shall program the RAF word length register
	 * accordingly
	 */
    if ((len & 0x3UL) == 0UL)
		words = LEN_TO_RAF_WORDS(len);
	else
		words = LEN_TO_RAF_WORDS(len) + 1UL;

	qspi_trace("raf set words: %d\n", words);
	raf_set_words(words);
	raf_set_watermark(RAF_WATERMARK_64_WORDS);

	ret = raf_start();
    if(ret != ERR_OK) {
        goto EXIT;
    }

    /* TODO: In case of fifo is empty but busy bit stuck at 1
       then this becomes a busy loop and will never come out.
       So, it is required to add timeout for that case as well */

	do {
        status = reg_read32(QSPI_RAF_STATUS);
        fifo_empty = ((status & (uint32_t)QSPI_RAF_STATUS_RAF_STATUS_FIFO_EMPTY_MASK)
                        >> (uint32_t)QSPI_RAF_STATUS_RAF_STATUS_FIFO_EMPTY_SHIFT);
        busy = ((status & (uint32_t)QSPI_RAF_STATUS_RAF_STATUS_SESSION_BUSY_MASK)
                        >> (uint32_t)QSPI_RAF_STATUS_RAF_STATUS_SESSION_BUSY_SHIFT);
		if (fifo_empty == 0UL) {
			fifo_data = raf_read_data();
			if (remain >= RAF_WORD_LEN) {
				*((uint32_t *)(data + count)) = fifo_data;
				count += RAF_WORD_LEN;
				remain = len - count;
			} else {
				for (i = 0UL; i < remain; i++) {
					data[count + i] = (int8_t)(fifo_data & 0xffUL);
					fifo_data = fifo_data >> 8;
				}
				remain = 0UL;
				count += i;
			}
		} else if ((fifo_empty != 0UL) && (busy == 0UL))
			break;
	} while (count < len);

    ret = raf_stop();
    if(ret != ERR_OK) {
        goto EXIT;
    }



    /* FIXME: Tight loop timeout to be removed later. */
    while ((raf_busy() != 0UL) && (j > 0UL)) {
		qspi_trace("RAF busy..\n");
        j--;
	}

    if(j == 0UL) {
        LOG_ERROR("RAF busy bit...timeout!!\n");
    }

EXIT:
	return ret;
}


int raf_read(uint32_t addr, char * const data, uint32_t len)
{
    uint32_t i;
    uint32_t addrAligned;
    uint32_t addrAlignOffset;
    char loc_data[4];
    uint32_t buf_widx = 0UL;
    int32_t ret;

	qspi_trace("raf_read: addr: 0x%x len: %d\n", addr, len);

	if ((data == NULL) || (len == 0UL)) {
		ret = ERR_INVAL;
        goto EXIT;
    }

    /* Check for unaligned address */
    addrAlignOffset = (addr & 0x3UL);
    if (addrAlignOffset == 0UL) {
        ret = raf_read_align_addr(addr, data, len);

    } else {
        addrAligned = (addr & (~0x3UL));
        ret = raf_read_align_addr(addrAligned, loc_data, RAF_WORD_LEN);
        if (ret == ERR_OK) {
            for (i = addrAlignOffset; (i < RAF_WORD_LEN) && (buf_widx < len); i++) {
                data[buf_widx] = loc_data[i];
                buf_widx++;
            }
            addrAligned += RAF_WORD_LEN;
            if (len > buf_widx) {
                ret = raf_read_align_addr(addrAligned,
                                        &(data[buf_widx]), (len - buf_widx));
            }
        }
    }
EXIT:
    return ret;
}

#elif __ENABLE_FLASH_QSPI_READ_BSPI_MM__
int mm_read(uint32_t addr, char *data, uint32_t len)
{
	uint32_t i, j, rem_bytes, words;
	uint32_t cnt;
	uint32_t uint32_t_sz = sizeof(uint32_t);
	uint32_t tmp;
	uint32_t *mm_addr = (uint32_t *)(FLASH_MEM_MAP_START_ADDR);
	uint32_t *buff_addr;

	if ((data == NULL) || (len == 0UL) || (addr >= FLASH_SIZE))
		return ERR_INVAL;

	if (qspi_init_done == 0L)
		return ERR_NOINIT;

	/* Read data byte-wise if address is un-aligned to 4 byte words */

	words = addr / uint32_t_sz;
	rem_bytes = addr % uint32_t_sz;

	mm_addr += words;

	if (rem_bytes > 0UL) {

		tmp = *mm_addr;
		for (i = rem_bytes; (i < uint32_t_sz) && (len > 0UL); i++) {

			*data = (int8_t)((tmp >> (8UL * i)) & 0xFFUL);
			data++;
			len--;
		}
		mm_addr++;

	}
	buff_addr = (uint32_t *)data;

	/* Get 4 Byte aligned words & read those */
	words = len / uint32_t_sz;
	rem_bytes = len % uint32_t_sz;

	/* For CRC calculation, read flash in terms of Prefetch buffer size.
	 * Start & end address of flash should be aligned to prefetch buffer
	 * size.
	 * If it is required to read flash size greater than prefetch buffer
	 * size then break it in terms of prefetch buffer size.
	 *
	 * Steps:
	 * 1. Set raf_num_words = Prefetch buffer size in words -
	 *			Words to be ignored for CRC calculation.
	 * 2. Read flash address.
	 * 3. If required to read more, then repeat step 1 & 2. Update
	 *    raf_num_words as per need.
	 *
	 * Note: Words to be ignored for CRC calculation shall include
	 * flash command,flash address, dummy cycles and/or data bytes.
	 */

	raf_num_words += PREFETCH_BUFF_SZ_IN_WORDS -
			BITS2WORDS((crc_get_idle_cycle_cnt() -
			QSPI_CRC_IDLE_CYCLE_COUNT_NO_IGNORE));

	for (i = 0UL; i < words; ) {
		if (i == 0UL) {
			raf_set_words(raf_num_words);
		} else if (i == PREFETCH_BUFF_SZ_IN_WORDS) {
			crc_set_idle_cycle_cnt(
				QSPI_CRC_IDLE_CYCLE_COUNT_NO_IGNORE);
			raf_num_words += PREFETCH_BUFF_SZ_IN_WORDS;
			raf_set_words(raf_num_words);
		}
		else {
			raf_num_words += PREFETCH_BUFF_SZ_IN_WORDS;
			raf_set_words(raf_num_words);
		}

		cnt = (words < (i + PREFETCH_BUFF_SZ_IN_WORDS)) ?
			(words - i) : PREFETCH_BUFF_SZ_IN_WORDS;

		for (j = 0UL; j < cnt; j++) {
			*buff_addr = *mm_addr;
			mm_addr++;
			buff_addr++;
			i++;
		}
	}

	/* Read byte wise data if length is unaligned */
	if(rem_bytes > 0UL) {
		tmp = *mm_addr;
		data = (char *) buff_addr;

		for (i = 0UL; i < rem_bytes; i++) {

			*data = (int8_t)(tmp & 0xFFUL);
			data++;
			tmp >>= 8UL;
		}
	}

	return ERR_OK;
}
#else
#error "ERROR: None of the QSPI BSPI read mode enabled!!"
#endif /* __ENABLE_FLASH_QSPI_READ_BSPI_MM__ */

int bspi_read_dual(uint32_t addr, char *data, uint32_t len)
{
	if ((data == NULL) || (len == 0UL))
		return ERR_INVAL;

	if (qspi_init_done == 0L)
		return ERR_NOINIT;

	qspi_trace("bspi_read_dual++\n");
#ifdef __ENABLE_FLASH_QSPI_READ_BSPI_RAF__
	return raf_read(addr, data, len);
#elif __ENABLE_FLASH_QSPI_READ_BSPI_MM__
	return mm_read(addr, data, len);
#endif
}

int bspi_read_quad(uint32_t addr, char *data, uint32_t len)
{
	if ((data == NULL) || (len == 0UL))
		return ERR_INVAL;

	if (qspi_init_done == 0L)
		return ERR_NOINIT;

	qspi_trace("bspi_read_quad++\n");
#ifdef __ENABLE_FLASH_QSPI_READ_BSPI_RAF__
	return raf_read(addr, data, len);
#elif __ENABLE_FLASH_QSPI_READ_BSPI_MM__
	return mm_read(addr, data, len);
#endif
}

int qspi_init(void)
{
	uint32_t reg;

	qspi_trace("qspi_init++\n");

	if(qspi_init_done == 1L)
		return ERR_BUSY;

	reg_write32((uint32_t)QSPI_MSPI_SPCR0_LSB,
		QSPI_SPBR & QSPI_MSPI_SPCR0_LSB_MSPI_SPCR0_LSB_SPBR_MASK );

	/* configure QSPI in master mode3 */

	reg = reg_read32((uint32_t)QSPI_MSPI_SPCR0_MSB);
	reg |= ((uint32_t)QSPI_MSPI_SPCR0_MSB_MSPI_SPCR0_MSB_CPHA_MASK) |
			((uint32_t)QSPI_MSPI_SPCR0_MSB_MSPI_SPCR0_MSB_CPOL_MASK) |
			((uint32_t)QSPI_MSPI_SPCR0_MSB_MSPI_SPCR0_MSB_MSTR_MASK);

	reg_write32((uint32_t)QSPI_MSPI_SPCR0_MSB, reg);
	reg = reg_read32((uint32_t)QSPI_MSPI_SPCR0_MSB);

	qspi_trace("SPCR0_MSB: %x\n", reg);

	qspi_config_gpio();

    /* Enable QSPI single read mode by default */
    bspi_enable_single_output();

	qspi_init_done = 1L;

	return ERR_OK;
}
