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

#include "ee_internal.h"
#include "bcm/utils/inc/bcm_delay.h"

/* Indirect reg access size */
#define REG_CTRL_SIZE_8BIT      0x0000U
#define REG_CTRL_SIZE_16BIT     0x0001U
#define REG_CTRL_SIZE_32BIT     0x0002U
#define REG_CTRL_SIZE_64BIT     0x0003U

/* Indirect reg access type */
#define REG_CTRL_WR_READ        0x0000U
#define REG_CTRL_WR_WRITE       0x0004U

/* Indirect reg auto increment */
#define REG_CTRL_AUTO_OFF       0x0000U

/* Indirect reg access commit */
#define REG_CTRL_RW_COMMIT      0x0010U

/* Indirect reg access read/write types */
#define REG_CTRL_READ           (uint16_t)(REG_CTRL_WR_READ | \
                                REG_CTRL_AUTO_OFF | \
                                REG_CTRL_RW_COMMIT)

/* switch supports 64bit width read/write only */
#define REG_CTRL_READ64         (uint16_t)(REG_CTRL_READ | REG_CTRL_SIZE_64BIT)

#define REG_CTRL_WRITE          (uint16_t)(REG_CTRL_WR_WRITE | \
                                REG_CTRL_AUTO_OFF | \
                                REG_CTRL_RW_COMMIT)

#define REG_CTRL_WRITE64        (uint16_t)(REG_CTRL_WRITE | REG_CTRL_SIZE_64BIT)

#ifdef DEBUG
#define SWITCH_ADDR_CHECK(addr) {   \
        if ((addr < SWITCH_START_ADDR) || (addr > SWITCH_END_ADDR))    \
            LOG_ERROR("Invalid switch reg address(0x%x)\n", addr);  \
    }
#else
#define SWITCH_ADDR_CHECK(addr)
#endif

static inline int32_t switch_read_start(uint32_t addr, uint16_t cmd)
{
    uint32_t i = 0UL;
    int32_t ret = ERR_OK;

    /* write register address */
    reg_write16(IND_ACC_RDB_IND_REGS_ADDR_CPU_L16, (uint16_t)addr);
    reg_write16(IND_ACC_RDB_IND_REGS_ADDR_CPU_H16, (uint16_t)(addr>>16));

    /* commit read operation */
    reg_write16(IND_ACC_RDB_IND_REGS_CTRL_CPU_L16, cmd);

    /* poll on command status reg for done bit */
    while ((!(reg_read16(IND_ACC_RDB_IND_REGS_CTRL_CPU_L16) &
                IND_ACC_RDB_IND_REGS_CTRL_CPU_L16_DONE_MASK)) &&
		(i < SW_RD_START_TIMEOUT)) {
	i++;
	tdelay(SW_RD_START_DELAY_TICK_CNT);
    }

    if (i >= SW_RD_START_TIMEOUT) {
        ret = ERR_TIMEOUT;
    }

    return ret;
}

static inline int32_t switch_write_start(uint32_t addr, uint16_t cmd)
{
    uint32_t i = 0UL;
    int32_t ret = ERR_OK;

    /* write reg address */
    reg_write16(IND_ACC_RDB_IND_REGS_ADDR_CPU_L16, (uint16_t)addr);
    reg_write16(IND_ACC_RDB_IND_REGS_ADDR_CPU_H16, (uint16_t)(addr >> 16));

    /* commit write operation */
    reg_write16(IND_ACC_RDB_IND_REGS_CTRL_CPU_L16, cmd);

    /* poll on command status reg for done bit */
    while ((!(reg_read16(IND_ACC_RDB_IND_REGS_CTRL_CPU_L16) &
                IND_ACC_RDB_IND_REGS_CTRL_CPU_L16_DONE_MASK)) &&
		(i < SW_WR_START_TIMEOUT)) {
	i++;
	tdelay(SW_WR_START_DELAY_TICK_CNT);
    }

    if (i >= SW_WR_START_TIMEOUT) {
        ret = ERR_TIMEOUT;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_read64(uint32_t addr, uint64_t *value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);
    if (status == E_OK) {

        ret = switch_read_start(addr, REG_CTRL_READ64);
        if(ret == ERR_OK) {

            *value = \
                     ((((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_H16)) << 48) |
                      (((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_L16)) << 32) |
                      (((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16)) << 16) |
                      ((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16)));
        }

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_read64: Unable to unlock IndirectAccessLock for "
                   "0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_read64: Unable to lock IndirectAccessLock for "
               "0x%x\n",addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR. Use switch_reg_read32_atomic instead */
int switch_reg_read32(uint32_t addr, uint32_t *value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        ret = switch_read_start(addr, REG_CTRL_READ64);
        if(ret == ERR_OK) {
            *value =
             ((uint32_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16) << 16) |
                reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16);
        }

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_read32: Unable to unlock IndirectAccessLock for "
                   "0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_read32: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}



/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_read16(uint32_t addr, uint16_t *value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        ret = switch_read_start(addr, REG_CTRL_READ64);
        if(ret == ERR_OK)
            *value = reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16);

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_read16: Unable to unlock IndirectAccessLock for "
                   "0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_read16: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_read8(uint32_t addr, uint8_t *value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        ret = switch_read_start(addr, REG_CTRL_READ64);
        if(ret == ERR_OK)
            *value = (uint8_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16);

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_read8: Unable to unlock IndirectAccessLock for "
                   "0x%x\n" ,addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_read8: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_write64(uint32_t addr, uint64_t value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16, (uint16_t)value);
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16, (uint16_t)(value >> 16));
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_L16, (uint16_t)(value >> 32));
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_H16, (uint16_t)(value >> 48));

        ret = switch_write_start(addr, REG_CTRL_WRITE64);

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_write64: Unable to unlock IndirectAccessLock for"
            " 0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_write64: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_write32(uint32_t addr, uint32_t value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16, (uint16_t)value);
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16, (uint16_t)(value >> 16));

        ret = switch_write_start(addr, REG_CTRL_WRITE64);

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_write32: Unable to unlock IndirectAccessLock for"
                   " 0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_write32: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_write16(uint32_t addr, uint16_t value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        /* write data to low data registers */
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16, value);

        ret = switch_write_start(addr, REG_CTRL_WRITE64);

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_write16: Unable to unlock IndirectAccessLock for"
                   " 0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_write16: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

/* WARNING: DO NOT CALL THIS API FROM ISR */
int switch_reg_write8(uint32_t addr, uint8_t value)
{
    int ret;
    StatusType status;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic */
    status = GetResource(IndirectAccessLock);

    if (status == E_OK) {
        reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16, value);

        ret  = switch_write_start(addr, REG_CTRL_WRITE64);

        status = ReleaseResource(IndirectAccessLock);
        if(status != E_OK) {
            printf("switch_reg_write8: Unable to unlock IndirectAccessLock for"
                   " 0x%x\n", addr);
            ret = ERR_NOPERM;
        }
    } else {
        printf("switch_reg_write8: Unable to lock IndirectAccessLock for "
               "0x%x\n", addr);
        ret = ERR_NOPERM;
    }

    return ret;
}

#ifdef SWITCH_ATOMIC_ACCESS

typedef enum
{
    READ_WRITE_ATOMIC_PASSED = 0,
    READ32_ATOMIC_FAILED = 1,
    READ64_ATOMIC_FAILED = 2,
    WRITE32_ATOMIC_FAILED = 4,
    WRITE64_ATOMIC_FAILED = 8,
}reg_access_errors_t;

reg_access_errors_t reg_access_error = READ_WRITE_ATOMIC_PASSED;
/* NOTE: Please use this API for switch access from ISR */
int switch_reg_read32_atomic(uint32_t addr, uint32_t *value)
{
    int ret;
    StatusType status;
    EE_UREG flags;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic by disabling interrupt */
    flags = EE_hal_suspendIRQ();

    ret = switch_read_start(addr, REG_CTRL_READ64);
    if(ret == ERR_OK) {
        *value =
            ((uint32_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16) << 16) |
            reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16);
    }
    else {
        ret = ERR_NOPERM;
        reg_access_error |= READ32_ATOMIC_FAILED;
    }

    EE_hal_resumeIRQ(flags);

    return ret;
}

/* NOTE: Please use this API for switch access from ISR */
int switch_reg_write32_atomic(uint32_t addr, uint32_t value)
{
    int ret;
    StatusType status;
    EE_UREG flags;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic by disabling interrupt */
    flags = EE_hal_suspendIRQ();

    reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16, (uint16_t)value);
    reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16, (uint16_t)(value >> 16));

    ret = switch_write_start(addr, REG_CTRL_WRITE64);

    if(ret != E_OK) {
        ret = ERR_NOPERM;
        reg_access_error |= WRITE32_ATOMIC_FAILED;
    }

    EE_hal_resumeIRQ(flags);
    return ret;
}

/* NOTE: Please use this API for switch access from ISR */
int switch_reg_read64_atomic(uint32_t addr, uint64_t *value)
{
    int ret;
    StatusType status;
    EE_UREG flags;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic by disabling interrupt */
    flags = EE_hal_suspendIRQ();

    ret = switch_read_start(addr, REG_CTRL_READ64);
    if(ret == ERR_OK) {

        *value = \
                 ((((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_H16)) << 48) |
                  (((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_L16)) << 32) |
                  (((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16)) << 16) |
                  ((uint64_t)reg_read16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16)));
    }
    else {
        ret = ERR_NOPERM;
        reg_access_error |= READ64_ATOMIC_FAILED;
    }

    EE_hal_resumeIRQ(flags);
    return ret;
}

/* NOTE: Please use this API for switch access from ISR */
int switch_reg_write64_atomic(uint32_t addr, uint64_t value)
{
    int ret;
    StatusType status;
    EE_UREG flags;

    SWITCH_ADDR_CHECK(addr);

    /* Make sure switch indirect access is atomic by disabling interrupt */
    flags = EE_hal_suspendIRQ();

    reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_L16, (uint16_t)value);
    reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_L_H16, (uint16_t)(value >> 16));
    reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_L16, (uint16_t)(value >> 32));
    reg_write16(IND_ACC_RDB_IND_REGS_DATA_CPU_H_H16, (uint16_t)(value >> 48));

    ret = switch_write_start(addr, REG_CTRL_WRITE64);

    if(ret != E_OK) {
        ret = ERR_NOPERM;
        reg_access_error |= WRITE64_ATOMIC_FAILED;
    }

    EE_hal_resumeIRQ(flags);
    return ret;
}
#endif