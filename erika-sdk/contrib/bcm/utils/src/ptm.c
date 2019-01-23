/*****************************************************************************
 Copyright 2017 Broadcom Limited.  All rights reserved.

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
#include "bcm/drivers/inc/flash.h"
#include "bcm/utils/inc/error.h"
#include "bcm/utils/inc/ptm.h"
#include "bcm/utils/inc/utils.h"
#include "bcm/utils/inc/edc.h"

#define FLASH_MAX_HW_ID     (1UL)

#define FALSE               (0UL)
#define TRUE                (1UL)

/** PTM State type */
typedef uint32_t PTM_StateType;
#define PTM_STATE_UNINIT        (0UL)   /**< PTM status uninitialized */
#define PTM_STATE_IDLE          (1UL)   /**< PTM status idle */
#define PTM_STATE_ERROR         (2UL)   /**< PTM status error */

#define IMG_TABLE_SIZE          (4UL * 1024UL) /* Img hdr + img entries will be
                                                  max 4KB */

#define FLASH_Read(aID, aAddr, aBuf, aSize) \
    flash_read(PTM_Comp.flashDev[aID], aAddr, (char *)aBuf, aSize);

#define FLASH_ID                (0UL)

typedef struct {
    PTM_LookupTblEntType lookupTbl[FLASH_MAX_HW_ID];
    flash_dev_t * flashDev[FLASH_MAX_HW_ID];
    uint32_t lookupTblSize;
    /** This is the index of lookupTbl where valid PT is found */
    uint32_t lookupIdx;
    PTM_StateType state;
    uint8_t ptTbl[PT_SIZE];
    uint8_t imgTbl[IMG_TABLE_SIZE];
    PTM_PTEntryType * ptEntry;
} PTM_CompType;

static PTM_CompType PTM_Comp = {
    .state = PTM_STATE_UNINIT,
    .flashDev[0UL] = NULL,
    .ptEntry = NULL,
};

static const PTM_PTHdrType *const PTHdr =
    (const PTM_PTHdrType *const)PTM_Comp.ptTbl;
static PTM_ImgHdrType *const ImgHdr = (PTM_ImgHdrType *const)PTM_Comp.imgTbl;

static PTM_ImgEntryType *PTM_IsCfgImgPresent(PTM_ImgIDType aImgID)
{
    uint32_t i;
    PTM_ImgEntryType *imgEntry =
        (PTM_ImgEntryType *)(PTM_Comp.imgTbl + IMG_HDR_STRUCT_SZ);

    for (i = 0UL; i < ImgHdr->numImgs; i++) {
        if (imgEntry->imgType == aImgID) {
            break;
        }
        imgEntry++;
    }
    if (i == ImgHdr->numImgs) {
        imgEntry = NULL;
    }

    return imgEntry;
}

static uint32_t PTM_IsImgIDValid(uint16_t aImgID)
{
    uint32_t retVal;

    if (aImgID == PT_ENTRY_PID_IMG_ID_APP_CFG) {
        retVal = TRUE;
    } else {
        retVal = FALSE;
    }

    return retVal;
}

static int32_t PTM_ReadAndValidateImg(PTM_ImgIDType aImgID,
                                      PTM_SizeType *const aImgSize,
                                      PTM_AddrType aAddr)
{
    int32_t retVal;
    uint32_t edcFlag;
    PTM_FlashIDType flashID;
    PTM_FlashAddrType addr;
    PTM_ImgEntryType *imgEntry;

    imgEntry = PTM_IsCfgImgPresent(aImgID);
    if (NULL != imgEntry) {
        if (0UL == imgEntry->actualSize) {
            retVal = ERR_NOT_FOUND;
            goto err;
        }
        if (imgEntry->actualSize > *aImgSize) {
            retVal = ERR_NOMEM;
            goto err;
        }
    } else {
        retVal = ERR_NOT_FOUND;
        goto err;
    }

    flashID = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashID;
    addr = PTM_Comp.ptEntry->addr + imgEntry->flashOffset;
    /* Read Img */
    *aImgSize = imgEntry->actualSize;
    retVal = FLASH_Read(flashID, addr, (char *)aAddr, imgEntry->actualSize);
    if (ERR_OK != retVal) {
        goto err;
    }
    /* Validate Img */
    edcFlag = ((imgEntry->flags & IMG_ENTRY_FLAG_ERR_DET_CODE_MASK) >>
            IMG_ENTRY_FLAG_ERR_DET_CODE_SHIFT);
    if (ERR_DET_CRC == edcFlag) {
        retVal = EDC_ValidateCRC(aAddr, imgEntry->actualSize,
                imgEntry->errDetCode);
    } else if (ERR_DET_CHECKSUM == edcFlag) {
        retVal = EDC_ValidateChcksm(aAddr, imgEntry->actualSize,
                imgEntry->errDetCode);
    } else {
        retVal = ERR_OK;
    }

err:
    return retVal;
}

static int32_t PTM_ReadAndValidateImgEntries(const PTM_PTEntryType *const aPTEntry)
{
    int32_t retVal;
    uint32_t edcFlag;
    uint32_t size;
    PTM_FlashIDType flashID;

    size = (ImgHdr->numImgs * IMG_ENTRY_STRUCT_SZ);
    if (size > (IMG_TABLE_SIZE - IMG_HDR_STRUCT_SZ)) {
        retVal = ERR_NOMEM;
        goto err;
    }
    flashID = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashID;
    /* Read Img Entries */
    retVal = FLASH_Read(flashID, (aPTEntry->addr + IMG_HDR_STRUCT_SZ),
            &PTM_Comp.imgTbl[IMG_HDR_STRUCT_SZ], size);
    if (ERR_OK != retVal) {
        goto err;
    }
    /* Validate Img Entries */
    edcFlag = ((ImgHdr->flags & IMG_HDR_FLAG_ERR_DET_CODE_MASK) >>
            IMG_HDR_FLAG_ERR_DET_CODE_SHIFT);
    if (ERR_DET_CRC == edcFlag) {
        retVal = EDC_ValidateCRC((PTM_Comp.imgTbl + IMG_HDR_STRUCT_SZ), size,
                ImgHdr->errDetCode);
    } else if (ERR_DET_CHECKSUM == edcFlag) {
        retVal = EDC_ValidateChcksm((PTM_Comp.imgTbl + IMG_HDR_STRUCT_SZ), size,
                ImgHdr->errDetCode);
    } else {
        retVal = ERR_OK;
    }

err:
    return retVal;
}

static int32_t PTM_ReadAndValidateImgHdr(const PTM_PTEntryType *const aPTEntry)
{
    PTM_FlashIDType flashID;
    int32_t retVal;

    if ((aPTEntry->flags & PT_ENTRY_FLAG_SKIP_MASK) ==
            PT_ENTRY_FLAG_SKIP_MASK) {
        retVal = ERR_CANCELLED;
        goto err;
    }
    flashID = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashID;
    /* Read Img Hdr */
    retVal = FLASH_Read(flashID, aPTEntry->addr, PTM_Comp.imgTbl,
            IMG_HDR_STRUCT_SZ);
    if (ERR_OK != retVal) {
        goto err;
    }
    /* Validate Img Hdr */
    if (IMG_MAGIC_NUM != ImgHdr->magicNumber) {
        retVal = ERR_INVAL_MAGIC;
    }

err:
    return retVal;
}

static int32_t PTM_ReadAndValidatePTEntries(void)
{
    int32_t retVal;
    uint32_t edcFlag;
    PTM_FlashAddrType flashAddr;
    PTM_FlashIDType flashID;
    uint32_t size = PTHdr->numEntries * PT_ENTRY_STRUCT_SZ;

    flashAddr = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashAddr +
        PT_HDR_STRUCT_SZ;
    flashID = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashID;
    /* Read PT Entries */
    retVal = FLASH_Read(flashID, flashAddr, &PTM_Comp.ptTbl[PT_HDR_STRUCT_SZ],
            size);
    if (ERR_OK != retVal) {
        goto err;
    }
    /* Validate PT Entries */
    edcFlag = ((PTHdr->flags & PT_HDR_FLAG_ERR_DET_CODE_MASK) >>
            PT_HDR_FLAG_ERR_DET_CODE_SHIFT);
    if (ERR_DET_CRC == edcFlag) {
        retVal = EDC_ValidateCRC(&PTM_Comp.ptTbl[PT_HDR_STRUCT_SZ], size,
                PTHdr->errDetCode);
    } else if (ERR_DET_CHECKSUM == edcFlag) {
        retVal = EDC_ValidateChcksm(&PTM_Comp.ptTbl[PT_HDR_STRUCT_SZ], size,
                PTHdr->errDetCode);
    } else {
        retVal = ERR_OK;
    }

err:
    return retVal;
}

static int32_t PTM_ReadAndValidatePTHdr(void)
{
    int32_t retVal;
    uint32_t size;
    PTM_FlashAddrType flashAddr;
    PTM_FlashIDType flashID;

    flashAddr = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashAddr;
    flashID = PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashID;
    /* Read PT Hdr */
    retVal = FLASH_Read(flashID, flashAddr, PTM_Comp.ptTbl, PT_HDR_STRUCT_SZ);
    if (ERR_OK != retVal) {
        goto err;
    }
    /* Validate PT Hdr */
    if (PT_MAGIC_NUM == PTHdr->magicNumber) {
        size = PTHdr->numEntries * PT_ENTRY_STRUCT_SZ;
        if (size != (PTHdr->totalSize - PT_HDR_STRUCT_SZ)) {
            retVal = ERR_NOMEM;
        } else {
            retVal = ERR_OK;
        }
    } else {
        retVal = ERR_INVAL_MAGIC;
    }

err:
    return retVal;
}

int32_t PTM_LoadImg(PTM_ImgIDType aImgID,
                    PTM_AddrType const aAddr,
                    PTM_SizeType *const aImgSize)
{
    int32_t retVal = ERR_UNKNOWN;
    PTM_ImgEntryType *imgEntry;
    uint32_t i;
    PTM_ImgIDType imgID;
    PTM_PTEntryType *ptEntry =
        (PTM_PTEntryType *)(PTM_Comp.ptTbl + PT_HDR_STRUCT_SZ);


    if (PTM_STATE_UNINIT == PTM_Comp.state) {
        retVal = ERR_NOINIT;
        goto err;
    }
    if (PTM_STATE_ERROR == PTM_Comp.state) {
        retVal = ERR_INVAL_STATE;
        goto err;
    }
    if ((NULL == aAddr) || (NULL == aImgSize)) {
        retVal = ERR_INVAL;
        goto err;
    }

    /* Load img header and entries for app configs */
    for (i = 0UL; i < PTHdr->numEntries; i++) {
        imgID = ((ptEntry->pid & PT_ENTRY_PID_IMG_ID_MASK) >>
                PT_ENTRY_PID_IMG_ID_SHIFT);
        if (PT_ENTRY_PID_IMG_ID_APP_CFG == imgID) {
            retVal = PTM_ReadAndValidateImgHdr(ptEntry);
            if (ERR_OK == retVal) {
                retVal = PTM_ReadAndValidateImgEntries(ptEntry);
                if (ERR_OK == retVal) {
                    /* Verify is aImgID present */
                    imgEntry = PTM_IsCfgImgPresent(aImgID);
                    if (NULL != imgEntry) {
                        PTM_Comp.ptEntry = ptEntry;
                        break;
                    }
                }
            }
        }
        ptEntry++;
    }
    if (i == PTHdr->numEntries) {
        PTM_Comp.state = PTM_STATE_ERROR;
        retVal = ERR_NOT_FOUND;
    } else {
        retVal = PTM_ReadAndValidateImg(aImgID, aImgSize, aAddr);
    }
err:
    return retVal;
}

int32_t PTM_Init(const PTM_CfgType *const aConfig)
{
    uint32_t i = 0UL, j;
    int32_t retVal = ERR_UNKNOWN;
    const PTM_LookupTblEntType *cfgtbl;
    PTM_PTHdrType *ptHdr;
    PTM_LookupTblEntType *lookupTbl = &PTM_Comp.lookupTbl[0];

    if (NULL == aConfig) {
        retVal = ERR_INVAL;
        goto err;
    }
    if (PTM_STATE_UNINIT != PTM_Comp.state) {
        retVal = ERR_INVAL_STATE;
        goto err;
    }

    /* Get flash device and check if flash is present */
    PTM_Comp.flashDev[FLASH_ID] = flash_get_dev();
    if (NULL == PTM_Comp.flashDev[FLASH_ID]) {
        retVal = ERR_NODEV;
        goto err;
    }

    retVal = flash_present(PTM_Comp.flashDev[FLASH_ID]);
    if (ERR_OK != retVal) {
        PTM_Comp.flashDev[FLASH_ID] = 0UL;
        retVal = ERR_NODEV;
        goto err;
    }

    if (NULL != aConfig->ptLoadAddr) {
        ptHdr = (PTM_PTHdrType *)(aConfig->ptLoadAddr);
        if (PT_MAGIC_NUM == ptHdr->magicNumber) {
            memcpy((void *)PTM_Comp.ptTbl, (void *)aConfig->ptLoadAddr,
                    ptHdr->totalSize);
        }
    } else {
        if (NULL != aConfig->lookupTbl) {
            cfgtbl = aConfig->lookupTbl;
            for (i = 0UL; i < aConfig->lookupTblSize; i++) {
                lookupTbl[i].flashID = cfgtbl[i].flashID;
                lookupTbl[i].flashAddr = cfgtbl[i].flashAddr;
            }
            PTM_Comp.lookupTblSize = aConfig->lookupTblSize;
        } else {
            retVal = ERR_INVAL;
            goto err;
        }
    }
    /**
     * Search through all the 4 redundant copies to find a valid partition table
     */
    for (i = 0UL; i < PTM_Comp.lookupTblSize; i++) {
        for (j = 0UL; j < NUM_PT_COPIES; j++) {
            retVal = PTM_ReadAndValidatePTHdr();
            if (ERR_OK == retVal) {
                retVal = PTM_ReadAndValidatePTEntries();
                if (ERR_OK == retVal) {
                    PTM_Comp.state = PTM_STATE_IDLE;
                    break;
                }
            }
            /* Go to next PT */
            PTM_Comp.lookupTbl[PTM_Comp.lookupIdx].flashAddr += PT_SIZE;
        }
        if (PTM_STATE_IDLE == PTM_Comp.state) {
            break;
        }

        (PTM_Comp.lookupIdx)++;
    }

    if (i == PTM_Comp.lookupTblSize) { /* No valid PT header found !! */
        PTM_Comp.state = PTM_STATE_ERROR;
        retVal = ERR_NOT_FOUND;
    }
err:
    return retVal;
}
