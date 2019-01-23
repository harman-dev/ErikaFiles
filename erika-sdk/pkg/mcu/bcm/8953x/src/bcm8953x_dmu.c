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
#include "mcu/bcm/8953x/inc/bcm8953x_dmu.h"

int dmu_init(void)
{
    return E_OK;
}

uint32_t dmu_get_cpuclk_freq(void)
{
	uint32_t freq = 0UL;
	uint32_t reg = reg_read32(DMU_DMU_CLK_SEL);

	reg = (reg & ((uint32_t)DMU_DMU_CLK_SEL_DMU_CPUCLK_SEL_MASK)) >>
		((uint32_t)DMU_DMU_CLK_SEL_DMU_CPUCLK_SEL_SHIFT);

	switch (reg) {
	case DMU_CPUCLK_SRC_REFCLK:
		freq = REF_CLOCK;
		break;
	case DMU_CPUCLK_SRC_PLLCLK:
		freq = PLL_TAP4_CLOCK;
		break;
	case DMU_CPUCLK_SRC_PLLCLK_DIV_2:
		freq = PLL_TAP4_CLOCK / 2UL;
		break;
	case DMU_CPUCLK_SRC_PLLCLK_DIV_4:
		freq = PLL_TAP4_CLOCK / 4UL;
		break;
	/*
	 * As DMU_DMU_CLK_SEL_DMU_CPUCLK_SEL_MASK = 0x00000003,
	 * So all the cases are covered. Because of this default
	 * is intentionaly left.
	 */

	}
	return freq;
}

uint32_t dmu_get_hclk_freq(void)
{
	uint32_t freq = 0UL;
	uint32_t reg = reg_read32(DMU_DMU_CLK_SEL);
	uint32_t cpuclk = dmu_get_cpuclk_freq();

	/* if cpuclk is 25MHz ref, HCLK is force to 25MHz
	*/
	if (cpuclk == REF_CLOCK) {
		return REF_CLOCK;
    } else {
        reg = (reg & ((uint32_t)DMU_DMU_CLK_SEL_DMU_HCLK_SEL_MASK)) >>
            ((uint32_t)DMU_DMU_CLK_SEL_DMU_HCLK_SEL_SHIFT);

        switch (reg) {
        case DMU_HCLK_SRC_CPUCLK_DIV_1:
            freq = cpuclk;
            break;
        case DMU_HCLK_SRC_CPUCLK_DIV_2:
            freq = cpuclk / 2UL;
            break;
        case DMU_HCLK_SRC_CPUCLK_DIV_3:
            freq = cpuclk / 3UL;
            break;
        case DMU_HCLK_SRC_CPUCLK_DIV_4:
            freq = cpuclk / 4UL;
            break;
        }
    }
    return freq;
}

uint32_t dmu_get_pclk_freq(void)
{
    uint32_t hclk;
	uint32_t freq = 0UL;
	uint32_t reg = 0UL;
	uint32_t cpuclk = dmu_get_cpuclk_freq();

	/* if cpuclk is 25MHz ref, PCLK is force to 25MHz
	*/
	if (cpuclk == REF_CLOCK) {
		freq = REF_CLOCK;
    } else {
        reg = reg_read32(DMU_DMU_CLK_SEL);
        hclk = dmu_get_hclk_freq();

        reg = (reg & ((uint32_t)DMU_DMU_CLK_SEL_DMU_PCLK_SEL_MASK)) >>
                ((uint32_t)DMU_DMU_CLK_SEL_DMU_PCLK_SEL_SHIFT);

        switch (reg) {
        case DMU_PCLK_SRC_HCLK_DIV_1:
            freq = hclk;
            break;
        case DMU_PCLK_SRC_HCLK_DIV_2:
            freq = hclk / 2UL;
            break;
        case DMU_PCLK_SRC_HCLK_DIV_3:
            freq = hclk / 3UL;
            break;
        }
    }
    return freq;
}

uint32_t dmu_get_qclk_freq(void)
{
	uint32_t freq = 0UL;
	uint32_t reg;
	uint32_t cpuclk = dmu_get_cpuclk_freq();

	/* if cpuclk is 25MHz ref, QCLK is force to 25MHz
	*/
	if (cpuclk == REF_CLOCK) {

		freq = REF_CLOCK;

    } else {
        reg = reg_read32(DMU_DMU_CLK_SEL);
        reg = (reg & ((uint32_t)DMU_DMU_CLK_SEL_DMU_QCLK_SEL_MASK)) >>
            ((uint32_t)DMU_DMU_CLK_SEL_DMU_QCLK_SEL_SHIFT);

        switch (reg) {
        case DMU_QCLK_SRC_CPUCLK_DIV_3:
            freq = cpuclk / 3UL;
            break;
        case DMU_QCLK_SRC_CPUCLK_DIV_4:
            freq = cpuclk / 4UL;
            break;
        case DMU_QCLK_SRC_CPUCLK_DIV_5:
            freq = cpuclk / 5UL;
            break;
        case DMU_QCLK_SRC_CPUCLK_DIV_10:
            freq = cpuclk / 10UL;
            break;
        /*
         * As DMU_DMU_CLK_SEL_DMU_QCLK_SEL_MASK = 0x0000000c,
         * So all the cases are covered. Because of this default
         * is intentionaly left.
         */
        }
    }

    return freq;
}

uint32_t dmu_set_qclk_freq(uint32_t freq_value)
{
	uint32_t div_value_near;
	dmu_qclk_src_t div_value = DMU_QCLK_SRC_CPUCLK_DIV_10;
	uint32_t reg = reg_read32(DMU_DMU_CLK_SEL);
	uint32_t cpuclk = dmu_get_cpuclk_freq();
    uint32_t set_freq_value;

	/* if cpuclk is 25MHz ref, QCLK is force to 25MHz
	*/
	if (cpuclk == REF_CLOCK) {

		set_freq_value = REF_CLOCK;

    } else {
        div_value_near = (cpuclk / freq_value);

        if(div_value_near < 3UL)
            div_value_near = 3UL;
        else if (div_value_near > 10UL)
            div_value_near = 10UL;
        else if (div_value_near > 5UL)
            div_value_near = 5UL;

        set_freq_value = cpuclk / div_value_near;

        switch (div_value_near) {
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
        }

        reg = (reg & (~((uint32_t)DMU_DMU_CLK_SEL_DMU_QCLK_SEL_MASK))) |
            (div_value << ((uint32_t)DMU_DMU_CLK_SEL_DMU_QCLK_SEL_SHIFT));

        reg_write32(DMU_DMU_CLK_SEL, reg);
    }

	return set_freq_value;
}
