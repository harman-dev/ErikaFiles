
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
#include "bcm/drivers/inc/flash.h"
#include "bcm/drivers/inc/qspi.h"
#include "bcm/drivers/inc/qspi_flash.h"
#include "ee_internal.h"

#define DUMMY_DATA		(0x0)

#define MSPI_XFER_CONT		(0x1)
#define MSPI_XFER_STOP		(0x0)


static char tx_buf[MSPI_FIFO_LEN];
static char rx_buf[MSPI_FIFO_LEN];


int qspiflash_set_speed(flash_dev_t *dev, uint32_t speed)
{

	return ERR_NOSUPPORT;
}

int qspiflash_get_speed(flash_dev_t *dev, uint32_t *speed)
{

	return ERR_NOSUPPORT;
}

static int qspiflash_read_status(flash_dev_t *dev, uint8_t *status)
{
	int ret;
	tx_buf[0] = (int8_t)FLASH_CMD_RDSR;
	tx_buf[1] = (int8_t)DUMMY_DATA;

	ret = mspi_xfer(tx_buf, rx_buf, 2UL, MSPI_XFER_STOP);
	if (ret != ERR_OK)
		return ret;

	status[0]= (uint8_t)rx_buf[1];

	return ret;

}

static int qspiflash_write_enable(flash_dev_t *dev, int enable)
{
	uint8_t status;
	int ret;

	/* send WREN/WDIS command */
	if (enable != 0L)
		tx_buf[0] = (int8_t)FLASH_CMD_WREN;
	else
		tx_buf[0] = (int8_t)FLASH_CMD_WRDI;

	ret = mspi_xfer(tx_buf, NULL, 1UL, MSPI_XFER_STOP);
	if (ret != ERR_OK)
		goto EXIT;

	do {
		ret = qspiflash_read_status(dev, &status);
		if (ret != ERR_OK)
			goto EXIT;

		if ((enable != 0L) && ((status & FLASH_STATUS_WEL_MASK) != 0))
			break;
		else if ((enable == 0L) && ((status & FLASH_STATUS_WEL_MASK) == 0))
			break;
	} while (1);

EXIT:
	return ret;
}

int32_t qspiflash_write_status(flash_dev_t *dev, uint8_t mask)
{
    uint8_t status;
    int32_t ret;

    ret = qspiflash_read_status(dev, &status);
    if (ret == ERR_OK) {

        /* send WREN command first */
        ret = qspiflash_write_enable(dev, 1);
        if (ret == ERR_OK) {

            /* Send write status register command  */
            tx_buf[0] = (int8_t)FLASH_CMD_WRSR;
            tx_buf[1] = (int8_t)(status | mask);

            /* This the size of buffer is 2UL bytes as only
               command and status is written */
            ret = mspi_xfer(tx_buf, NULL, 2UL, MSPI_XFER_STOP);

            if(ret == ERR_OK) {
                /* wait for WIP bit to clear */
                do {
                    ret = qspiflash_read_status(dev, &status);
                } while (((status & FLASH_STATUS_WIP_MASK) ==
                        FLASH_STATUS_WIP_MASK) && (ret == ERR_OK));

                /* write enable latch bit should be cleared
                 * automatically at the end of write status
                 * command. But just in case, we clear it
                 * explicitly by writing WRDI command
                 */
                if(ret == ERR_OK) {
                    ret = qspiflash_write_enable(dev, 0);
                }
            }

        }
    }

    return ret;
}

static int qspiflash_chip_erase(flash_dev_t *dev)
{
	uint8_t status;
	int ret;

	/* send WREN command first */
	ret = qspiflash_write_enable(dev, 1);
	if (ret != ERR_OK)
		goto EXIT;

	/* Send bulk erase */
	tx_buf[0] = (int8_t)FLASH_CMD_BE;
	ret = mspi_xfer(tx_buf, NULL, 1UL, MSPI_XFER_STOP);
	if (ret != ERR_OK)
		goto EXIT;

	/* wait for WIP bit to clear */
	do {
		ret = qspiflash_read_status(dev, &status);
		if (ret != ERR_OK)
			goto EXIT;
	} while ((status & FLASH_STATUS_WIP_MASK) == FLASH_STATUS_WIP_MASK);

	/* write enable latch bit should be cleared
	 * automatically at the end of bulk erase
	 * command. But just in case, we clear it
	 * explicitly by writing WRDI command
	 */
	ret = qspiflash_write_enable(dev, 0);

EXIT:
	return ret;
}

static int qspiflash_sector_erase(flash_dev_t *dev, uint32_t addr)
{
	uint8_t status;
	int ret;

	/* send WREN command first */
	ret = qspiflash_write_enable(dev, 1);
	if (ret != ERR_OK)
		goto EXIT;

	/* Send sector erase */
	tx_buf[0] = (int8_t)FLASH_CMD_SE;
	tx_buf[1] = (int8_t)ADDRESS23_16(addr);
	tx_buf[2] = (int8_t)ADDRESS15_8(addr);
	tx_buf[3] = (int8_t)ADDRESS7_0(addr);
	ret = mspi_xfer(tx_buf, NULL, 4UL, MSPI_XFER_STOP);
	if (ret != ERR_OK)
		goto EXIT;

	/* wait for WIP bit to clear */
	do {
		ret = qspiflash_read_status(dev, &status);
		if (ret != ERR_OK)
			goto EXIT;
	} while ((status & FLASH_STATUS_WIP_MASK) == FLASH_STATUS_WIP_MASK);

	/* write enable latch bit should be cleared
	 * automatically at the end of sector erase
	 * command. But just in case, we clear it
	 * explicitly by writing WRDI command
	 */

	ret = qspiflash_write_enable(dev, 0);

EXIT:
	return ret;
}

#ifdef FLASH_SUBSECTOR_SUPPORT_ENABLE
static int qspiflash_subsector_erase(flash_dev_t *dev, uint32_t addr)
{
	uint8_t status;
	int ret;

	/* send WREN command first */
	ret = qspiflash_write_enable(dev, 1);
	if (ret != ERR_OK)
		goto EXIT;

	/* Send sub-sector erase */
	tx_buf[0] = (int8_t)FLASH_CMD_SSE;
	tx_buf[1] = (int8_t)ADDRESS23_16(addr);
	tx_buf[2] = (int8_t)ADDRESS15_8(addr);
	tx_buf[3] = (int8_t)ADDRESS7_0(addr);
	ret = mspi_xfer(tx_buf, NULL, 4UL, MSPI_XFER_STOP);
	if (ret != ERR_OK)
		goto EXIT;

	/* wait for WIP bit to clear */
	do {
		ret = qspiflash_read_status(dev, &status);
		if (ret != ERR_OK)
			goto EXIT;
	} while ((status & FLASH_STATUS_WIP_MASK) == FLASH_STATUS_WIP_MASK);

	/* write enable latch bit should be cleared
	 * automatically at the end of sector erase
	 * command. But just in case, we clear it
	 * explicitly by writing WRDI command
	 */

	ret = qspiflash_write_enable(dev, 0);

EXIT:
	return ret;
}
#endif

static int qspiflash_page_write(flash_dev_t *dev, uint32_t page_addr,
						const char *data, uint32_t len)
{
	uint32_t i = 0UL;
	uint32_t rem;
	uint32_t mul_16;
	uint32_t iter;
	uint8_t status;
	int ret;

	/* send WREN command first */
	ret = qspiflash_write_enable(dev, 1);
	if (ret != ERR_OK)
		goto EXIT;

	/* send PP command and request MSPI to keep CS asserted
	 */
	tx_buf[0] = (int8_t)FLASH_CMD_PP;
	tx_buf[1] = (int8_t)ADDRESS23_16(page_addr);
	tx_buf[2] = (int8_t)ADDRESS15_8(page_addr);
	tx_buf[3] = (int8_t)ADDRESS7_0(page_addr);

	ret = mspi_xfer(tx_buf, NULL, 4UL, MSPI_XFER_CONT);
	if (ret != ERR_OK)
		goto EXIT;

	mul_16 = (len & 0xFUL) ? 0UL : 1UL;
	iter = len / MSPI_FIFO_LEN;

	for (i = 0UL; i < iter; i++) {
		if ((i == (iter - 1UL)) && (mul_16 != 0UL)) {
			ret = mspi_xfer(&data[i * MSPI_FIFO_LEN], NULL,
				MSPI_FIFO_LEN, MSPI_XFER_STOP);
		}
		else {
			ret = mspi_xfer(&data[i * MSPI_FIFO_LEN], NULL,
					MSPI_FIFO_LEN, MSPI_XFER_CONT);
		}
		if (ret != ERR_OK)
			goto EXIT;
	}

	rem = len - (i * MSPI_FIFO_LEN);

	if (rem != 0UL) {
		ret = mspi_xfer(&data[i * MSPI_FIFO_LEN], NULL, rem,
				MSPI_XFER_STOP);
		if (ret != ERR_OK)
			goto EXIT;
	}

	do {
		ret = qspiflash_read_status(dev, &status);
		if (ret != ERR_OK)
			goto EXIT;
	} while ((status & FLASH_STATUS_WIP_MASK) == FLASH_STATUS_WIP_MASK);

	ret = qspiflash_write_enable(dev, 0L);

EXIT:

	return ret;
}

static int qspiflash_read_id(flash_dev_t *dev, flash_rdid_info_t *info)
{
	int ret = ERR_OK;

	tx_buf[0] = (int8_t)FLASH_CMD_RDID;
	tx_buf[1] = (int8_t)DUMMY_DATA;
	tx_buf[2] = (int8_t)DUMMY_DATA;
	tx_buf[3] = (int8_t)DUMMY_DATA;

	ret = mspi_xfer(tx_buf, rx_buf, 4UL, MSPI_XFER_STOP);
	if (ret != ERR_OK)
		return ret;

	info->manufac_id = (uint8_t)rx_buf[1];
	info->device_id[0] = (uint8_t)rx_buf[2];
	info->device_id[1] = (uint8_t)rx_buf[3];

	return ret;
}

static int32_t qspiflash_set_read_mode(flash_dev_t *dev)
{
    int32_t err = ERR_OK;

    switch (dev->mode) {
    case FLASH_MODE_SINGLE:
        bspi_enable_single_output();
        break;
    case FLASH_MODE_DUAL_OUT:
        bspi_enable_dual_output();
        break;
    case FLASH_MODE_QUAD_OUT:
        err = enable_flash_quad_mode(dev);
        if(err != ERR_OK) {
            break;
        }
        bspi_enable_quad_output();
        break;
    case FLASH_MODE_DUAL_IO:
    case FLASH_MODE_QUAD_IO:
    default:
        LOG_CRIT("qspiflash_set_read_mode: mode not supported\n");
        err = ERR_NOSUPPORT;
    }
    return err;
}

static int qspiflash_read(flash_dev_t *dev, uint32_t addr,
						char *buf, uint32_t len)
{
	int err = ERR_NOSUPPORT;

	switch (dev->mode) {
	case FLASH_MODE_SINGLE:
#ifdef __ENABLE_FLASH_QSPI_READ_BSPI_RAF__
		err = raf_read(addr, buf, len);
#elif __ENABLE_FLASH_QSPI_READ_BSPI_MM__
		err = mm_read(addr, buf, len);
#endif
		break;
	case FLASH_MODE_DUAL_OUT:
		err = bspi_read_dual(addr, buf, len);
		break;
	case FLASH_MODE_QUAD_OUT:
		err = bspi_read_quad(addr, buf, len);
		break;
	case FLASH_MODE_DUAL_IO:
	case FLASH_MODE_QUAD_IO:
	default:
		LOG_CRIT("qspiflash_read: mode not supported\n");
		err = ERR_NOSUPPORT;
	}
	return err;
}

static int qspiflash_fast_read(flash_dev_t *dev, uint32_t addr,
						char *buf, uint32_t len)
{
#ifdef __ENABLE_FLASH_QSPI_READ_BSPI_RAF__
	return raf_read(addr, buf, len);
#elif __ENABLE_FLASH_QSPI_READ_BSPI_MM__
	return mm_read(addr, buf, len);
#endif
}

int32_t qspiflash_init(flash_dev_t *dev, flash_mode mode)
{
    int32_t ret;

	if ((dev == NULL) || (mode >= FLASH_MODE_MAX_LIMITER))
		return ERR_INVAL;

	dev->set_speed = qspiflash_set_speed;
	dev->get_speed = qspiflash_get_speed;
	dev->read_id = qspiflash_read_id;
	dev->read = qspiflash_read;
	dev->fast_read = qspiflash_fast_read;
	dev->chip_erase = qspiflash_chip_erase;
	dev->sector_erase = qspiflash_sector_erase;
	dev->page_write = qspiflash_page_write;
#ifdef FLASH_SUBSECTOR_SUPPORT_ENABLE
	dev->subsector_erase = qspiflash_subsector_erase;
#endif
	dev->mode = mode;

	ret = qspi_init();
    if(ret == ERR_OK) {
        ret = qspiflash_set_read_mode(dev);
    }

    return ret;
}
