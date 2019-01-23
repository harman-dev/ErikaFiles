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

#include <stdint.h>
#include <string.h>
#include "ee_internal.h"
#include "bcm/drivers/inc/flash.h"
#include "bcm/utils/inc/flash_updater.h"
#include "bcm/utils/inc/edc.h"
#include "bcm/drivers/inc/crc.h"

/*
 * Macros
 */

#define FALSE       (0UL)
#define TRUE        (1UL)

/* 'AVBH' */
#define FUP_AVB_PRT_HDR_MGC     (0x41564248UL)
#define FUP_VPD_PRT_HDR_MGC     (0x56504448UL)

#define PT_VPD_IMG_ID           (0xBC00U)
#define PT_USR_PRT_TBL_IMG_ID   (0xBC01U)

#define USR_PRT_AVB_PRT_ID      (0xFFF0U)
#define USR_PRT_VPD_PRT_ID      (0xFFF1U)

#define USR_PRT_IDX(aPrtID)     ((aPrtID) & (0xFUL))

/*
 * Align to upper limit of the alignment value
 */
#define ALIGN(x, alignment)     \
        (((x) + ((alignment) - 1UL)) & (~((alignment) - 1UL)))

/*
 * Flash erase api/size selection based on sector/sub-sector
 */
#ifdef FLASH_SUBSECTOR_SUPPORT_ENABLE
#define FUP_FLASH_ERASE_SIZE    FLASH_SUBSECTOR_SIZE
#define FUP_FlashErase(x, y)    flash_subsector_erase(x, y)
#else
#define FUP_FLASH_ERASE_SIZE    FLASH_SECTOR_SIZE
#define FUP_FlashErase(x, y)    flash_sector_erase(x, y)
#endif

#define FUP_FLASH_PAGE_SIZE     FLASH_PAGE_SIZE
#define FUP_FLASH_SIZE          FLASH_SIZE

/*
 * Structs
 */
#define FUP_USR_PRT_TBL_MAX_ENTRY   (16UL)

/** User partition header type */
typedef struct {
    uint32_t magic;     /**< magic: 'AVBH' for AVB */
    uint32_t flags;     /**< flags: reserved for future expansion */
    uint32_t maxSize;   /**< maximum size of the partition */
    uint32_t size;      /**< current size of the partition */
    uint32_t crc;       /**< CRC value of the image */
    uint32_t rsvd1;     /**< reserved */
    uint32_t rsvd2;     /**< reserved */
    uint32_t rsvd3;     /**< reserved */
} FUP_UsrPrtHdrType;

/** User partition table entry type */
typedef struct {
    uint32_t rsvd1[3];  /**< reserved */
    uint32_t flashAddr; /**< Absolute flash address of user partition */
    uint32_t rsvd2;     /**< reserved */
    uint16_t prtID;     /**< User partition ID */
    uint16_t rsvd3;     /**< reserved */
    uint32_t rsvd4;     /**< reserved */
    uint32_t maxSize;   /**< User partition maximum size */
    uint32_t rsvd5;     /**< reserved */
} FUP_UsrPrtTblEntryType;

typedef struct {
    uint32_t prtCnt;    /**< Count of partition of this type */
    uint32_t prtState;  /**< Partition state valid (bit value: 1) otherwise 0 */
    uint16_t prtID;     /**< User partition ID */
} FUP_UsrPrtInfoType;

typedef struct {
    uint32_t state;
#define FUP_STATE_UNINT     (0UL)
#define FUP_STATE_INIT      (1UL)
    uint32_t usrPrtTblEntryNum;
    FUP_UsrPrtTblEntryType *const usrPrtTbl;
    FUP_UsrPrtInfoType usrPrtInfo[2UL];
} FUP_CompType;

/* User partition table */
#define FUP_USR_PRT_TBL_MAX_SIZE    (sizeof(FUP_UsrPrtTbl))
static FUP_UsrPrtTblEntryType FUP_UsrPrtTbl[FUP_USR_PRT_TBL_MAX_ENTRY];
static uint8_t FUP_Buf[FUP_FLASH_PAGE_SIZE];

/* FUP component info */
static FUP_CompType FUP_Comp = {
    .state = FUP_STATE_UNINT,
    .usrPrtTblEntryNum = 0UL,
    .usrPrtTbl = FUP_UsrPrtTbl,
    .usrPrtInfo[0UL].prtID = USR_PRT_AVB_PRT_ID,
    .usrPrtInfo[0UL].prtCnt = 0UL,
    .usrPrtInfo[0UL].prtState = 0UL,
    .usrPrtInfo[1UL].prtID = USR_PRT_VPD_PRT_ID,
    .usrPrtInfo[1UL].prtCnt = 0UL,
    .usrPrtInfo[1UL].prtState = 0UL,
};

static int32_t FUP_GetUsrPrtTbl(void)
{
    int32_t retVal;
    PTM_SizeType usrPrtTblSize = FUP_USR_PRT_TBL_MAX_SIZE;

    retVal = PTM_LoadImg(PT_USR_PRT_TBL_IMG_ID,
                        (uint8_t *)FUP_Comp.usrPrtTbl,
                        &usrPrtTblSize);
    if (ERR_OK != retVal) {
        LOG_INFO("%s: PTM_LoadImg error: 0x%x\n", __func__, retVal);
    } else {
        FUP_Comp.usrPrtTblEntryNum =
                        usrPrtTblSize/sizeof(FUP_UsrPrtTblEntryType);
    }

    return retVal;
}

static int32_t FUP_GetUsrPrtInfo(uint16_t aPrtID,
                                uint32_t aPrtCnt,
                                uint32_t *const aPrtAddr,
                                uint32_t *const aPrtMaxSize,
                                uint32_t *const aIsLast)
{
    int32_t retVal = ERR_OK;
    uint32_t i, prtCnt = 0UL;

    if (0UL == FUP_Comp.usrPrtTblEntryNum) {
        retVal = ERR_NOT_FOUND;
    } else {
        for (i = 0UL; i < FUP_Comp.usrPrtTblEntryNum; i++) {
            if (aPrtID == FUP_Comp.usrPrtTbl[i].prtID) {
                prtCnt++;
                if (prtCnt == aPrtCnt) {
                    *aPrtAddr = FUP_Comp.usrPrtTbl[i].flashAddr;
                    *aPrtMaxSize = FUP_Comp.usrPrtTbl[i].maxSize;
                }
            }
        }

        if (0UL != prtCnt) {
            if (prtCnt == aPrtCnt) {
                *aIsLast = TRUE;
            } else {
                *aIsLast = FALSE;
            }
        } else {
            retVal = ERR_NOT_FOUND;
        }
    }
    return retVal;
}

static int32_t FUP_GetUsrPrtHdr(uint16_t aPrtID,
                                uint32_t aPrtAddr,
                                FUP_UsrPrtHdrType *const aPrtHdr)
{
    int32_t retVal;
    flash_dev_t * flsDev;

    flsDev = flash_get_dev();
    retVal = flash_read(flsDev, aPrtAddr, (char *)(aPrtHdr),
                        sizeof(FUP_UsrPrtHdrType));
    if (ERR_OK == retVal) {
        if (USR_PRT_AVB_PRT_ID == aPrtID){
            if ((FUP_AVB_PRT_HDR_MGC != aPrtHdr->magic)
                    || (FUP_FLASH_SIZE <= aPrtHdr->size)) {
                retVal = ERR_DATA_INTEG;
            }
        } else {
            if (USR_PRT_VPD_PRT_ID == aPrtID){
                if ((FUP_VPD_PRT_HDR_MGC != aPrtHdr->magic)
                        || (FUP_FLASH_SIZE <= aPrtHdr->size)) {
                    retVal = ERR_DATA_INTEG;
                }
            } else {
                retVal = ERR_NOT_FOUND;
            }
        }
    }
    return retVal;
}

static int32_t FUP_GetUsrPrtData(uint16_t aPrtID,
                                uint32_t aPrtAddr,
                                uint8_t *const aBuf,
                                uint32_t *const aBufSize)
{
    int32_t retVal;
    FUP_UsrPrtHdrType prtHdr;
    uint32_t flsAddr = aPrtAddr;
    flash_dev_t * flsDev = flash_get_dev();

    /* Read partition header */
    retVal = FUP_GetUsrPrtHdr(aPrtID, flsAddr, &prtHdr);
    if (ERR_OK != retVal) {
        goto err;
    }

    if (*aBufSize < prtHdr.size) {
        retVal = ERR_NOMEM;
        goto err;
    }

    memset(aBuf, 0xFF, *aBufSize);
    /* Read partition data */
    flsAddr += sizeof(FUP_UsrPrtHdrType);
    retVal = flash_read(flsDev, flsAddr, (char *)(aBuf), prtHdr.size);
    if (ERR_OK != retVal) {
        goto err;
    }

    /* Validate CRC */
    retVal = EDC_ValidateCRC(aBuf, prtHdr.size, prtHdr.crc);
    if (ERR_OK == retVal) {
        *aBufSize = prtHdr.size;
    }

err:
    return retVal;
}

static void FUP_ValidatePrt(uint16_t aPrtID)
{
    int32_t retVal;
    uint32_t i, j;
    uint32_t prtAddr;
    uint32_t prtMaxSize;
    uint32_t isLastPrt;
    uint32_t flsAddr;
    uint32_t rdSize;
    uint32_t crc;
    FUP_UsrPrtHdrType prtHdr;
    flash_dev_t * flsDev = flash_get_dev();

    for (i = 0UL; i < FUP_Comp.usrPrtTblEntryNum; i++) {
        retVal = FUP_GetUsrPrtInfo(aPrtID, (i + 1UL),
                &prtAddr, &prtMaxSize, &isLastPrt);
        if (ERR_OK != retVal) {
            break;
        }
        FUP_Comp.usrPrtInfo[USR_PRT_IDX(aPrtID)].prtCnt = (i + 1UL);
        if (TRUE == isLastPrt) {
            FUP_Comp.usrPrtInfo[USR_PRT_IDX(aPrtID)].prtState |=
                (PRT_STATE_PRT_PERM_RO_VAL << (i + PRT_STATE_PRT_PERM_SHIFT));
        }

        retVal = FUP_GetUsrPrtHdr(aPrtID, prtAddr, &prtHdr);
        if (ERR_OK != retVal) {
            continue;
        }

        crc = (~0UL);
        flsAddr = (prtAddr + sizeof(FUP_UsrPrtHdrType));
        for (j = 0UL; j < prtHdr.size; j += rdSize) {
            /* Read partition data */
            rdSize = (prtHdr.size - j);
            rdSize = (rdSize > FUP_FLASH_PAGE_SIZE) ?
                        FUP_FLASH_PAGE_SIZE : rdSize;
            retVal = flash_read(flsDev, flsAddr, (char *)(FUP_Buf), rdSize);
            if (ERR_OK != retVal) {
                break;
            }
            flsAddr += rdSize;

            /* Calculate CRC */
            crc = EDC_CalculateCRC(FUP_Buf, rdSize, crc);
        }
        if ((ERR_OK == retVal) && (prtHdr.crc == crc)) {
            FUP_Comp.usrPrtInfo[USR_PRT_IDX(aPrtID)].prtState |=
                                    (PRT_STATE_PRT_STATE_VALID_VAL << i);
        } else {
            FUP_Comp.usrPrtInfo[USR_PRT_IDX(aPrtID)].prtState &=
                                    ~(PRT_STATE_PRT_STATE_VALID_VAL << i);
        }
    }

    return ;
}

static int32_t FUP_GetPrtHdr(prt_type_t aPrtType,
                            prt_hdr_t *const aPrtHdr)
{
    int retVal = ERR_NOT_FOUND;
    uint32_t i;
    FUP_UsrPrtHdrType prtHdr;
    uint32_t prtID;
    uint32_t prtAddr = ~(0x0UL);
    uint32_t prtMaxSize = 0UL;
    uint32_t isLastPrt;

    if (PRT_TYPE_AVB == aPrtType) {
        prtID = USR_PRT_AVB_PRT_ID;
    } else {
        if (PRT_TYPE_VPD == aPrtType) {
            prtID = USR_PRT_VPD_PRT_ID;
        }
    }
    for (i = 0UL; i < FUP_Comp.usrPrtTblEntryNum; i++) {
        retVal = FUP_GetUsrPrtInfo(prtID, (i + 1UL), &prtAddr,
                                    &prtMaxSize, &isLastPrt);
        if (ERR_OK != retVal) {
            break;
        }

        if ((FUP_FLASH_SIZE < prtAddr) || (0UL == prtMaxSize)) {
            retVal = ERR_NOT_FOUND;
            break;
        }

        retVal = FUP_GetUsrPrtHdr(prtID, prtAddr, &prtHdr);
        if (ERR_OK == retVal) {
            aPrtHdr->prt_data_sz = prtHdr.size;
            aPrtHdr->prt_sz = prtMaxSize;
            break;
        }
    }

    return retVal;
}

static int32_t FUP_ErasePrt(uint32_t aPrtAddr, uint32_t aPrtSize)
{
    int32_t retVal = ERR_OK;
    flash_dev_t *flsDev;
    uint32_t addr = aPrtAddr;
    uint32_t loopCnt = (aPrtSize/FUP_FLASH_ERASE_SIZE);
    uint32_t i;

    if ((aPrtSize > FUP_FLASH_SIZE)
            || ((aPrtSize & (FUP_FLASH_ERASE_SIZE - 1UL)) != 0UL)) {
        retVal = ERR_INVAL;
        goto err;
    }

    flsDev = flash_get_dev();
    for (i = 0UL; i < loopCnt; i++) {
        retVal = FUP_FlashErase(flsDev, addr);
        if(ERR_OK != retVal) {
            break;
        }
        addr += FUP_FLASH_ERASE_SIZE;
    }

err:
    return retVal;
}

static int32_t FUP_WritePrt(uint32_t aPrtAddr,
                            FUP_UsrPrtHdrType *const aPrtHdr,
                            uint8_t *const aData,
                            uint32_t aDataSize)
{
    int32_t retVal;
    uint32_t i, size;
    uint32_t flsAddr = aPrtAddr;
    uint8_t buf[FUP_FLASH_PAGE_SIZE];
    flash_dev_t *flsDev = flash_get_dev();

    size = sizeof(FUP_UsrPrtHdrType);
    retVal = flash_write(flsDev, flsAddr, (char *)aPrtHdr, size);
    if (ERR_OK != retVal) {
        goto err;
    }

    retVal = flash_read(flsDev, flsAddr, (char *)buf, size);
    if (ERR_OK != retVal) {
        goto err;
    }

    retVal = memcmp(buf, aPrtHdr, size);
    if (0L != retVal) {
        retVal = ERR_DATA_INTEG;
        goto err;
    }

    for (i = 0UL; i < aDataSize; i += size) {
        flsAddr += size;
        size = aDataSize - i;
        size = (size > FUP_FLASH_PAGE_SIZE)? FUP_FLASH_PAGE_SIZE: size;
        retVal = flash_write(flsDev, flsAddr, (char *)&aData[i], size);
        if (ERR_OK != retVal) {
            break;
        }

        retVal = flash_read(flsDev, flsAddr, (char *)buf, size);
        if (ERR_OK != retVal) {
            break;
        }

        retVal = memcmp(&buf, &aData[i], size);
        if (0L != retVal) {
            retVal = ERR_DATA_INTEG;
            break;
        }
    }

err:
   return retVal;
}

int32_t fup_update_prt_data(void *const aData,
                            uint32_t aSize,
                            prt_type_t aPrtType)
{
    int retVal;
    uint32_t i;
    FUP_UsrPrtHdrType prtHdr;
    uint32_t prtID;
    uint32_t prtAddr;
    uint32_t isLastPrt;
    uint32_t prtMaxSize;

    if(FUP_STATE_UNINT == FUP_Comp.state) {
        retVal = ERR_NOINIT;
        goto err;
    }

    if ((NULL == aData) || (0UL == aSize)
            || (aPrtType < 0L) || (aPrtType >= PRT_TYPE_LAST)) {
        retVal = ERR_INVAL;
        goto err;
    }
    if (PRT_TYPE_AVB == aPrtType) {
        prtID = USR_PRT_AVB_PRT_ID;
    } else {
        if (PRT_TYPE_VPD == aPrtType) {
            prtID = USR_PRT_VPD_PRT_ID;
        } else {
            retVal = ERR_INVAL;
            goto err;
        }
    }

    if ((FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtCnt < 2UL)
            || (((FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtState & (~1UL))
                    & (~PRT_STATE_PRT_PERM_MASK)) == 0UL)) {
        retVal = ERR_NOSUPPORT;
        goto err;
    }

    for (i = 0UL; i < FUP_Comp.usrPrtTblEntryNum; i++) {
        retVal = FUP_GetUsrPrtInfo(prtID, (i + 1UL),
                                    &prtAddr, &prtMaxSize, &isLastPrt);
        if (ERR_OK != retVal) {
            break;
        }

        if ((FUP_FLASH_SIZE < prtAddr) || (0UL == prtMaxSize)) {
            retVal = ERR_NOT_FOUND;
            break;
        }

        retVal = FUP_GetUsrPrtHdr(prtID, prtAddr, &prtHdr);
        if (ERR_OK == retVal) {
            break;
        }
    }

    if ((ERR_OK != retVal) || (i == FUP_Comp.usrPrtTblEntryNum)) {
        retVal = ERR_NOT_FOUND;
        goto err;
    }

    /* Validate partition header */
    if ((PRT_TYPE_AVB == aPrtType) && (FUP_AVB_PRT_HDR_MGC != prtHdr.magic)) {
        retVal = ERR_DATA_INTEG;
        goto err;
    }

    retVal = FUP_GetUsrPrtInfo(prtID, 1UL, &prtAddr, &prtMaxSize, &isLastPrt);
    if (ERR_OK != retVal) {
        goto err;
    }

    if (TRUE == isLastPrt) {
        retVal = ERR_NOSUPPORT;
        goto err;
    }

    if ((prtMaxSize - sizeof(FUP_UsrPrtHdrType))  < aSize) {
        retVal = ERR_NOMEM;
        goto err;
    }

    /* Erase partition */
    retVal = FUP_ErasePrt(prtAddr, prtMaxSize);
    if (ERR_OK != retVal) {
        goto err;
    }

    /* Compute CRC and update header */
    prtHdr.crc = EDC_CalculateCRC(aData, aSize, (~0UL));
    prtHdr.size = aSize;

    /* Write the partition data */
    retVal = FUP_WritePrt(prtAddr, &prtHdr, aData, aSize);

err:
    return retVal;
}

/*
 * Function to get partition header.
 */
int32_t fup_get_prt_hdr(prt_type_t aPrtType,
                        prt_hdr_t *const aPrtHdr)
{
    int32_t retVal;

    if(FUP_STATE_UNINT == FUP_Comp.state) {
        retVal = ERR_NOINIT;
        goto err;
    }

    if((aPrtType < 0L) || (aPrtType >= PRT_TYPE_LAST) || (NULL == aPrtHdr)) {
        retVal = ERR_INVAL;
        goto err;
    }

    retVal = FUP_GetPrtHdr(aPrtType, aPrtHdr);

err:
    return retVal;
}

/*
 * Function to get partition data.
 */
int32_t fup_get_prt_data(void *const aData,
                        uint32_t *const aSize,
                        prt_type_t aPrtType,
                        uint32_t *const aIsPrtRO)
{
    int32_t retVal = ERR_UNKNOWN;
    uint32_t i;
    uint32_t prtID;
    uint32_t prtAddr;
    uint32_t prtMaxSize;
    uint32_t isLastPrt;

    if(FUP_STATE_UNINT == FUP_Comp.state) {
        retVal = ERR_NOINIT;
        goto err;
    }

    if ((NULL == aData) || (NULL == aSize) || (0UL == *aSize)
            || (aPrtType < 0L) || (aPrtType >= PRT_TYPE_LAST)) {
        retVal = ERR_INVAL;
        goto err;
    }

    if (PRT_TYPE_AVB == aPrtType) {
        prtID = USR_PRT_AVB_PRT_ID;
    } else {
        if (PRT_TYPE_VPD == aPrtType) {
            prtID = USR_PRT_VPD_PRT_ID;
        }
    }
    for (i = 0UL; i < FUP_Comp.usrPrtTblEntryNum; i++) {
        retVal = FUP_GetUsrPrtInfo(prtID, (i + 1UL),
                &prtAddr, &prtMaxSize, &isLastPrt);
        if (ERR_OK != retVal) {
            break;
        }

        if ((FUP_FLASH_SIZE < prtAddr) || (0UL == prtMaxSize)) {
            retVal = ERR_NOT_FOUND;
            break;
        }

        retVal = FUP_GetUsrPrtData(prtID, prtAddr, aData, aSize);
        if (ERR_OK == retVal) {
            break;
        }

        if (TRUE == isLastPrt) {
            retVal = ERR_NOT_FOUND;
            break;
        }
    }
    *aIsPrtRO = isLastPrt;
err:
    return retVal;
}

int32_t fup_get_prt_state(prt_type_t aPrtType,
                        uint32_t *const aPrtCnt,
                        prtStateType *const aPrtState)
{
    int32_t retVal = ERR_OK;
    uint32_t prtID;

    if(FUP_STATE_UNINT == FUP_Comp.state) {
        retVal = ERR_NOINIT;
        goto err;
    }

    if ((aPrtType < 0L) || (aPrtType >= PRT_TYPE_LAST)
            || (NULL == aPrtCnt || (NULL == aPrtState))) {
        retVal = ERR_INVAL;
        goto err;
    }

    if (PRT_TYPE_AVB == aPrtType) {
        prtID = USR_PRT_AVB_PRT_ID;
    } else {
        if (PRT_TYPE_VPD == aPrtType) {
            prtID = USR_PRT_VPD_PRT_ID;
        }
    }

    if (USR_PRT_AVB_PRT_ID == FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtID) {
        *aPrtCnt = FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtCnt;
        *aPrtState = FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtState;
    } else {
        if (USR_PRT_VPD_PRT_ID ==
                FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtID) {
            *aPrtCnt = FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtCnt;
            *aPrtState = FUP_Comp.usrPrtInfo[USR_PRT_IDX(prtID)].prtState;
        }
    }

err:
    return retVal;
}

/*
 * This function needs to be called from system_init().
 */
void fup_init(void)
{
    int retVal;
    flash_dev_t *flsDev;

    if(FUP_STATE_UNINT != FUP_Comp.state) {
        LOG_CRIT("%s: fup_init called multiple times!!\n", __func__);
        goto err;
    }

    /* Validate the presence of valid Flash device */
    flsDev = flash_get_dev();
    retVal = flash_present(flsDev);
    if (ERR_OK != retVal) {
        LOG_CRIT("%s: No flash device (error: 0x%x)!!\n", __func__, retVal);
        goto err;
    }

    /* Get user partition table */
    retVal = FUP_GetUsrPrtTbl();
    if (ERR_OK == retVal) {
        /* Verify the data integrity of all AVB partitions */
        FUP_ValidatePrt(USR_PRT_AVB_PRT_ID);
        /* Verify the data integrity of all VPD partitions */
        FUP_ValidatePrt(USR_PRT_VPD_PRT_ID);
    }

    FUP_Comp.state = FUP_STATE_INIT;

err:
    return ;
}
