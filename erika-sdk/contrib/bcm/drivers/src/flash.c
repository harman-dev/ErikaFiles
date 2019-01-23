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
#include "bcm/drivers/inc/flash.h"
#include "ee_internal.h"

/* FLASH_TRACE to be defined only for debug/development */
/* #define FLASH_TRACE */

#ifdef FLASH_TRACE
#define flash_trace(...)	LOG_DEBUG(__VA_ARGS__)
#else
#define flash_trace(...)
#endif

#define HARMAN_CONFIG

#ifdef HARMAN_CONFIG
#define MSP_MANUFACTURER_ID         (0x20)
#define MSP_DEVICE_ID_BYTE1         (0x20)
#define MSP_DEVICE_ID_BYTE2         (0x15)

#define ISSI_MANUFACTURER_ID     (0x9D)
#define ISSI_DEVICE_ID_BYTE1     (0x60)
#define ISSI_DEVICE_ID_BYTE2     (0x15)
#endif

static flash_dev_t flash_device;

#ifdef __ENABLE_FLASH_QSPI__
extern int qspiflash_init(flash_dev_t *dev, flash_mode mode);
#endif

int flash_read_id(flash_dev_t *dev, flash_rdid_info_t *info)
{
	if ((dev != &flash_device) || (info == NULL)) {
		flash_trace("ERROR: invalid flash device\n");
		return ERR_INVAL;
	}

	if (dev->read_id != NULL)
		return dev->read_id(dev, info);
	else
		return ERR_NOINIT;
}

int flash_present(flash_dev_t *dev)
{
	flash_rdid_info_t info;
	int ret = ERR_OK;

	if (dev != &flash_device) {
		flash_trace("ERROR: invalid params\n");
		return ERR_INVAL;
	}

	memset(&info, 0L, sizeof(flash_rdid_info_t));

	ret = flash_read_id(dev, &info);
	if (ret != 0)
		return ret;

	flash_trace("Flash RDID: 0x%x\n", ((info.manufac_id << 16) |
					(info.device_id[0] << 8) |
					(info.device_id[1] << 0)));
#ifndef HARMAN_CONFIG
	if ((info.manufac_id == ((uint8_t)MANUFACTURER_ID)) &&
			(info.device_id[0] == ((uint8_t)DEVICE_ID_BYTE1)) &&
			(info.device_id[1] == ((uint8_t)DEVICE_ID_BYTE2)))
		return ERR_OK;
	else {
		flash_trace("ERROR: Different Flash ID found\n");
		return ERR_NODEV;
	}
#else
        if (((info.manufac_id == ((uint8_t)MSP_MANUFACTURER_ID)) &&
                        (info.device_id[0] == ((uint8_t)MSP_DEVICE_ID_BYTE1)) &&
                        (info.device_id[1] == ((uint8_t)MSP_DEVICE_ID_BYTE2))) ||
                ((info.manufac_id == ((uint8_t)ISSI_MANUFACTURER_ID)) &&
                        (info.device_id[0] == ((uint8_t)ISSI_DEVICE_ID_BYTE1)) &&
                        (info.device_id[1] == ((uint8_t)ISSI_DEVICE_ID_BYTE2))))
        {
                flash_trace(" Flash RDID: 0x%x Matched\n", ((info.manufac_id << 16) |
                                (info.device_id[0] << 8) |
                                 (info.device_id[1] << 0)));
                return ERR_OK;
        }
        else {
                flash_trace("ERROR: Different Flash ID found\n");
                return ERR_NODEV;
        }
#endif
}

int flash_subsector_erase(flash_dev_t *dev, uint32_t subsector_start)
{
	if ((dev != &flash_device) || (subsector_start >= FLASH_SIZE))
		return ERR_INVAL;

#ifdef FLASH_SUBSECTOR_SUPPORT_ENABLE
	if (dev->subsector_erase != NULL)
		return dev->subsector_erase(dev, subsector_start);
	else
		return ERR_NOINIT;
#else
		return ERR_NOSUPPORT;
#endif
}

int flash_sector_erase(flash_dev_t *dev, uint32_t sector_start_addr)
{
	if ((dev != &flash_device) || (sector_start_addr >= FLASH_SIZE))
		return ERR_INVAL;

	if (dev->sector_erase != NULL)
		return dev->sector_erase(dev, sector_start_addr);
	else
		return ERR_NOINIT;
}

int flash_bulk_erase(flash_dev_t *dev)
{
	if (dev != &flash_device)
		return ERR_INVAL;

	if (dev->chip_erase != NULL)
		return dev->chip_erase(dev);
	else
		return ERR_NOINIT;
}

int flash_write(flash_dev_t *dev, uint32_t addr,  const char * const data,
		uint32_t len)
{
	uint32_t addr_offset = 0UL;
	uint32_t write_len = 0UL;
    uint32_t idx = 0UL;
	int ret = ERR_OK;

	if((dev != &flash_device) || (data == NULL) ||
		(addr >= FLASH_SIZE) || (len > FLASH_SIZE)
		|| (len == 0UL) || ((addr + len) > FLASH_SIZE))
		return ERR_INVAL;

	if (dev->page_write == NULL)
		return ERR_NOINIT;

	while (idx < len) {

		addr_offset = addr % FLASH_PAGE_SIZE;
		write_len = FLASH_PAGE_SIZE - addr_offset;
		if ((len - idx) < write_len)
			write_len = (len - idx);

		ret = dev->page_write(dev, addr, &data[idx], write_len);
		addr += write_len;
        idx += write_len;
	}

	return ret;
}

int flash_read(flash_dev_t *dev, uint32_t addr, char *buf, uint32_t len)
{
	if ((dev != &flash_device) || (buf == NULL) ||
		(len == 0UL) || (len > FLASH_SIZE) ||
		(addr >= FLASH_SIZE) || ((addr + len) > FLASH_SIZE)) {

		flash_trace("ERROR: invalid params\n");
		return ERR_INVAL;
	}

	if (dev->read != NULL)
		return dev->read(dev, addr, buf, len);
	else
		return ERR_NOINIT;
}

int flash_fast_read(flash_dev_t *dev, uint32_t addr, char *buf, uint32_t len)
{
	if ((dev != &flash_device) || (buf == NULL) ||
		(len == 0UL) || (len > FLASH_SIZE) ||
		(addr >= FLASH_SIZE) || ((addr + len) > FLASH_SIZE)) {

		flash_trace("ERROR: invalid params\n");
		return ERR_INVAL;
	}

	if (dev->fast_read != NULL)
		return dev->fast_read(dev, addr, buf, len);
	else
		return ERR_NOINIT;
}

flash_dev_t * flash_get_dev(void)
{
	return &flash_device;
}

int flash_get_speed(flash_dev_t *dev, uint32_t *speed)
{
	if ((dev != &flash_device) || (speed == NULL))
		return ERR_INVAL;

	if (dev->get_speed != NULL)
		return dev->get_speed(dev, speed);
	else
		return ERR_NOINIT;
}

int flash_set_speed(flash_dev_t *dev, uint32_t speed)
{
	if ((dev != &flash_device) || (speed == 0UL))
		return ERR_INVAL;

	if (dev->set_speed != NULL )
		return dev->set_speed(dev, speed);
	else
		return ERR_NOINIT;
}

int flash_init(void)
{
	flash_dev_t *dev = NULL;
	int ret = ERR_OK;

	flash_trace("flash_init++\n");

	dev = &flash_device;

#if defined ( __ENABLE_FLASH_QSPI__ )

	ret = qspiflash_init(dev, FLASH_RD_MODE);
	if(ret != ERR_OK)
		LOG_CRIT("%s: qspiflash_init failed\n", __func__);

	dev->type = FLASH_TYPE_QSPI;
#else
#error "ERROR: None of the flash controller enabled!!"
#endif

	return ret;
}
