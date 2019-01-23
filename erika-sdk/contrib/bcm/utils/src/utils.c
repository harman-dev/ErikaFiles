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
#include <stdint.h>
#include <stdlib.h>
#include "bcm/utils/inc/utils.h"

/*
 * @api
 * parse_ipv4
 *
 * @brief
 * Parse a command line argument as an IPV4 address xxx.xxx.xxx.xxx
 *
 * @param=buf - string to parse
 * @param=value - pointer to where to return the value
 *
 * @returns 0 on error, !0 on success
 */
int parse_ipv4(char *buf, uint32_t *value)
{
    int i;
    uint32_t octet;
    uint32_t ipaddr = 0UL;
    char *ptr = buf;
    int32_t ret = 1L;

    for (i = 0L; i < 4L; ++i) {
        if (i == 0L) {
            octet = (uint32_t)strtol(buf, &ptr, 10L);
        }
        else {
            octet = (uint32_t)strtol(&ptr[1], &ptr, 10L);
        }
        if (i == 3L) {
            if ((*ptr != '\0') || (octet > 255UL)) {
                ret = 0L;
                break;
            }
        }
        else {
            if ((*ptr != '.') || (octet > 255UL)) {
                ret = 0L;
                break;
            }
        }
        ipaddr = (ipaddr << 8) | octet;
    }
    if (ret != 0L) {
        *value = ipaddr;
    }
    return ret;
}

/*
 * @api
 * parse_ether
 *
 * @brief
 * Parse a command line argument as an Ethernet address xx:xx:xx:xx:xx:xx
 *
 * @param=buf - string to parse
 * @param=value - pointer to where to return the value
 *
 * @returns 0 on error, !0 on success
 */
int parse_ether(char *buf, uint8_t * const value)
{
    uint32_t i;
    uint32_t octet;
    char *ptr = buf;
    int32_t ret = 1L;

    for (i = 0UL; i < ETHER_ADDR_LEN; ++i) {
        if (i == 0UL) {
            octet = (uint32_t)strtol(buf, &ptr, 16L);
        }
        else {
            octet = (uint32_t)strtol(&ptr[1], &ptr, 16L);
        }
        if (i == (ETHER_ADDR_LEN - 1UL)) {
            if ((*ptr != '\0') || (octet > 255UL)) {
                ret = 0L;
                break;
            }
        }
        else {
            if ((*ptr != ':') || (octet > 255UL)) {
                ret = 0L;
                break;
            }
        }
        value[i] = (uint8_t)octet;
    }
    return ret;
}

/*
 * @api
 * parse_int
 *
 * @brief
 * Parse a command line argument as an int
 *
 * @param=buf - string to parse
 * @param=value - pointer to where to return the value
 *
 * @returns 0 on error, !0 on success
 */
int parse_int(char *buf, uint32_t *value)
{
    if (buf[0] == '0') {
        if( strtoll(buf, (char **) NULL, 10L) > 0xfffffffeLL){
            return 0; /* strtoll function returns long long value*/
        }
        if ((buf[1] == 'x') || (buf[1] == 'X')) {
            *value = (uint32_t)strtol(&buf[2], (char **) NULL, 16L);
        }
        else {
            *value = (uint32_t)strtol(&buf[1], (char **) NULL, 8L);
        }
    }
    else if( buf[0] <= '9' && buf[0] >= '0' &&
            strtoll(buf, (char **) NULL, 10L) < 0xffffffffLL){
        *value = (uint32_t)strtol(buf, (char **) NULL, 10L);
    }else {
        return 0;
    }
    return 1;
}

/*
 * @api
 * parse_signed_int
 *
 * @brief
 * Parse a command line argument as an int
 *
 * @param=buf - string to parse
 * @param=value - pointer to where to return the value
 *
 * @returns 0 on error, !0 on success
 */
int parse_signed_int(const char * const buf_ptr, int32_t *value)
{
    int sign = 1;
    const char *buf;

    if ( buf_ptr[0] == '-'){
      sign = -1;
      buf = &buf_ptr[1];
    } else {
      buf = buf_ptr;
    }

    if (buf[0] == '0') {
        if( strtoll(buf, (char **) NULL, 10L) > 0x7fffffffLL){
            return -1; /* strtoll function returns long long value*/
        }
        if ((buf[1] == 'x') || (buf[1] == 'X')) {
            *value = strtol(&buf[2], (char **) NULL, 16L);
        }
        else {
            *value = strtol(&buf[1], (char **) NULL, 8L);
        }
    }
    else if( buf[0] <= '9' && buf[0] >= '0' &&
            strtoll(buf, (char **) NULL, 10L) <= 0x7fffffffLL){
        *value = strtol(buf, (char **) NULL, 10L);
    }else {
        return -1;
    }
    *value = (*value) * sign;
    return 1;
}

/*
 * @api
 * parse_int64
 *
 * @brief
 * Parse a command line argument as an int64
 *
 * @param=buf - string to parse
 * @param=value - pointer to where to return the value
 *
 * @returns 0 on error, !0 on success
 */
int parse_int64(const char *buf, uint64_t *value)
{
    if (buf[0] == '0') {
        if ((buf[1] == 'x') || (buf[1] == 'X')) {
            *value = (uint64_t)strtoll(&buf[2], (char **) NULL, 16L);
        }
        else {
            *value = (uint64_t)strtoll(&buf[1], (char **) NULL, 8L);
        }
    }
    else {
        *value = (uint64_t)strtoll(buf, (char **) NULL, 10L);
    }
    return 1;
}
