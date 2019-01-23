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

/*@
 *
 * Purpose: Printing services
 *
 * Printing and formatting operations.
 */
#include <string.h>
#include <limits.h>
#include "bcm/utils/inc/print.h"
#include "bcm/utils/inc/log.h"
#include "bcm/drivers/inc/uart.h"


#define PRINTF_BUF_SIZE					256

/*@
 *  @section=print.c-static
 *  @title print.c file static variables
 */

/*@
 *
 * polled_printf - flag to use polling mode
 */
static uint32_t polled_printf = 0;


#define PUTC(ch)     (polled_printf ? uart_putc_polled(ch) : uart_putc(ch))

/*@
 * @endsection
 */

void printf_setmode(uint32_t mode)
{
	polled_printf = mode;
}

uint32_t  is_printf_polled(void)
{
	return polled_printf;
}

/*api
 * dump_buffer
 *
 * @brief
 * Dump a buffer of bytes with identifying information
 *
 * @param=name - name of the buffer
 * @param=data - pointer to the data
 * @param=len - length of the data
 *
 * @returns void
 *
 * @desc
 */
void dump_buffer(char *name, const uint8_t *data, int len)
{
    int i;

    LOG_INFO("%s: %d bytes @ %p .. %p", name, len, data, data + len - 1);
    for (i = 0; i < len; ++i) {
        LOG_INFO("%s%02x", (i % 16) ? " " : "\n", data[i]);
    }
    LOG_INFO("\n");
}

/*api
 * dump_buffer_words
 *
 * @brief
 * Dump a buffer of words with identifying information
 *
 * @param=name - name of the buffer
 * @param=data - pointer to the data
 * @param=len - length of the data
 *
 * @returns void
 *
 * @desc
 */
void dump_buffer_words(char *name, const uint32_t *data, int len)
{
    int i;

    LOG_INFO("%s: %d words @ %p .. %p", name, len, data, data + len - 1);
    for (i = 0; i < len; ++i) {
        LOG_INFO("%s%08x", (i % 4) ? " " : "\n", data[i]);
    }
    LOG_INFO("\n");
}

/*api
 * format_ether_addr
 *
 * @brief
 * Format an Ethernet address into a buffer
 *
 * @param=buf - buffer to write the string to
 * @param=addr - pointer to the Ethernet address
 *
 * @returns buf
 *
 * @desc
 */
char *format_ether_addr(char *buf, uint8_t *addr)
{
    (void)sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return buf;
}

/*api
 * format_ipv4_addr
 *
 * @brief
 * Format an IPv4 address into a buffer
 *
 * @param=buf - buffer to write the string to
 * @param=addr - pointer to the IPv4 address
 *
 * @returns buf
 *
 * @desc
 */
char *format_ipv4_addr(char *buf, uint32_t ipaddr)
{
    (void)sprintf(buf, "%d.%d.%d.%d",
                (ipaddr >> 24) & 0xffUL,
                (ipaddr >> 16) & 0xffUL,
                (ipaddr >> 8) & 0xffUL,
                (ipaddr >> 0) & 0xffUL);
    return buf;
}


/*api
 * mos_uart_print_string
 *
 * @brief
 * Output a NUL-terminated string to the UART
 *
 * @param=buf - pointer to the buffer to send
 *
 * @returns number of bytes sent, or 0 on failure
 *
 * @desc
 */
static int32_t uart_print_string(const char *buf)
{
    int32_t len = 0L;
    const char *ptr;

    if (uart_initialized() == 0UL) {
        /* The uart is not active - give up */
        return 0L;
    }

    for (ptr = buf; (*ptr != '\0'); ++ptr) {
        if (*ptr == '\n') {
            while (PUTC((uint8_t)'\r') != 0L) {
            }
        }
        while (PUTC((uint8_t)*ptr) != 0L) {
        }
        ++len;
    }

    return len;
}


/*
 * add_int_to_tbuf
 *
 * @brief
 * A helper function to put an integer in a buffer
 *
 * @returns pointer to first character in buffer
 */

char * add_int_to_tbuf(uint64_t value, int negative, int base, char pad, uint32_t width, char *end_of_buffer) {
    uint32_t cnt;
    const char *fmt_string = "0123456789abcdef";
    char *tptr = end_of_buffer;

    if (value == 0ULL) {
        *--tptr = '0';
    } else {
        while (value) {
            int n = (int32_t)(value %((uint64_t)base));
            *--tptr = fmt_string[n];
            value /= ((uint64_t)base);
        }
    }

    if (pad != '0') {
        /* leading minus sign if needed */
        if (negative != 0L) {
            *--tptr = '-';
        }
    }

    /* zero or space extend as needed */
    if(width > strlen(tptr)) {
        cnt = width - strlen(tptr);
    } else {
        cnt = 0UL;
    }

    while (cnt > 0UL) {
        *--tptr = pad;
           cnt--;
    }

    if (pad == '0') {
        /* leading minus sign if needed */
        if (negative != 0L) {
            *--tptr = '-';
        }
    }

    return tptr;
}

/*api
 * bcm_vsnprintf
 *
 * @brief
 * A stripped down version of vsprintf
 *
 * @param=buf - buffer to format the string into
 * @param=size - size of buf
 * @param=fmt - pointer to the format string
 * @param=args - the va_list of arguments to format
 *
 * @returns the length of the resulting string
 *
 * @desc
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    char ch;
    char *bptr = buf;
    char tbuf[96];

    const char *fptr = fmt;

    /* adjust size so it will not overflow an int */
    if (size >= (uint32_t)INT_MAX) {
        size = (uint32_t)INT_MAX;
    }

    while ((ch = *fptr++) != '\0') {
        int len_left = size - (bptr - buf) - 1L;  /* -1 to leave space for NUL at end */

        if (len_left <= 0L) {
            break;
        }

        if (ch == '%') {
            int len = 0L;
            int precision = 0L;
#ifdef UKERNEL_FP
            int has_precision = 0L;
#endif
            char pad = ' ';
            int pad_left = 1L;
            int arg_is_64bit = 0L;

            /* check for leading minus */
            if (*fptr == '-') {
                pad_left = 0L;
                ++fptr;
            }
            /* check for leading zeros */
            if (*fptr == '0') {
                pad = '0';
                ++fptr;
            }
            /* check for length */
            while ((*fptr >= '0') && (*fptr <= '9')) {
                len = (len * 10L) + (*fptr++ - '0');
            }

            if (*fptr == '.') {
#ifdef UKERNEL_FP
                /* note: precision only supported below for floating-point */
                has_precision = 1L;
#endif
                ++fptr;
                while ((*fptr >= '0') && (*fptr <= '9')) {
                    precision = (precision * 10L) + ((int32_t)*fptr++ - (int32_t)'0');
                }
            }

            /* check for size of argument "l" or "ll" */
            if (*fptr == 'l') {
                ++fptr;
                if (*fptr == 'l') {
                    ++fptr;
                    arg_is_64bit = 1L;
                }
            }
            /* determine format */
            switch (ch = *fptr++) {
            case '%':
                *bptr++ = '%';
                *bptr = '\0';
                break;

            case 'd':
            case 'p':
            case 'u':
            case 'x': {
                int64_t signed_value;
                uint64_t unsigned_value;
                char *tptr = tbuf + sizeof(tbuf);
                int negative = 0L;
                int base = ((ch == 'd') || (ch == 'u')) ? 10L : 16L;
                int cpy_len;
                *--tptr = '\0';

                /* pointers are 8 characters long by default */
                if ((ch == 'p') && (len == 0L)) {
                    len = 8L;
                    pad_left = 1L;
                }
                if ((arg_is_64bit != 0L)) {
                    signed_value = va_arg(args, int64_t);
                } else {
                    if (ch == 'd') {
                        signed_value = va_arg(args, int32_t);
                    } else {
                        signed_value = (unsigned)va_arg(args, uint32_t);
                    }
                }
                if ((ch == 'd') && (signed_value < 0LL)) {
                    negative = 1L;
                    unsigned_value = -signed_value;
                } else {
                    negative = 0L;
                    unsigned_value = (uint64_t)signed_value;
                }
                 /* pointers always pad with zeros */
                if (ch == 'p') {
                    pad = '0';
                }

                tptr = add_int_to_tbuf(unsigned_value, negative, base, pad, (pad_left != 0L) ? len : 0, tptr);

                /* format pointers with leading 0x */
                if (ch == 'p') {
                    *--tptr = 'x';
                    *--tptr = '0';
                }
                cpy_len = strlen(tptr);
                if (cpy_len > len_left) {
                    cpy_len = len_left;
                }
                strncpy(bptr, tptr, cpy_len);
                bptr += cpy_len;
                len_left -= cpy_len;

                /* zero or space extend as needed */
                if (pad_left == 0L) {
                    int cnt = len - cpy_len;
                    while ((cnt-- > 0L) && (len_left != 0L)) {
                        *bptr++ = pad;
                        len_left--;
                    }
                }
                break;
            }

            case 's': {
                char *tmp;

                tmp = va_arg(args, char *);
                int cpy_len;

                if ((len != 0L) && (pad_left != 0L)) {
                    int spaces = (int32_t)(len - strlen(tmp));
                    while ((spaces-- > 0L) && (len_left != 0L)) {
                        *bptr++ = ' ';
                        len_left--;
                    }
                }

                cpy_len = strlen(tmp);
                if (cpy_len > len_left) {
                    cpy_len = len_left;
                }
                strncpy(bptr, tmp, cpy_len);

                bptr += cpy_len;
                len_left -= cpy_len;

                if ((len != 0L) && (pad_left == 0L)) {
                    int spaces = len - cpy_len;
                    while ((spaces-- > 0L) && (len_left != 0L)) {
                        *bptr++ = ' ';
                        len_left--;
                    }
                }
                *bptr = '\0';
                break;
            }

            case 'c': {
                char tmp;

                tmp = va_arg(args, int);
                *bptr++ = tmp;
                *bptr = '\0';
                len_left--;
                break;
            }

#ifdef UKERNEL_FP
            /*BATMadd*/
            case 'e':
            case 'f':
            case 'g': {
                int negative = 0L;
                int base = 10L;
                double  d_value, orig_d_value;
                int exponent = 0L;
                int has_exponent = 0L;
                double d_remainder;
                uint64_t remainder=0ULL;
                int cpy_len;
                int i;
                char *tptr = tbuf + sizeof(tbuf);
                *--tptr = '\0';

                if (has_precision == 0L) {
                    precision = 6L;
                }

                d_value = (double)va_arg(args, double);

                if (d_value < 0) {
                    d_value = -d_value;
                    negative = 1L;
                }

                orig_d_value = d_value;

                if (ch != 'f') {
                    has_exponent = 1L`;
                    if (d_value != 0) {
                        while (d_value < 1) {
                            d_value *= 10;
                            exponent--;
                        }
                        while (d_value > 10) {
                            d_value /= 10;
                            exponent++;
                        }
                    }
                }

                if (ch == 'g') {
                    /* rule for %g:  like %e if exponent is less than -4 or greater than the precision.  Like %f otherwise */
                    if (exponent < -4L || exponent > precision) {
                        has_exponent = 1L;
                    } else {
                        has_exponent = 0L;
                        d_value = orig_d_value;
                    }
                }

                uint64_t int_value = (uint64_t)(d_value);
                d_remainder = d_value - int_value;

                for (i = 0L; i < precision; ++i) {
                    d_remainder *= 10;
                }
                remainder = (uint64_t) (d_remainder + 0.5);

                if (has_exponent != 0L) {
                    int exponent_negative = 0L;
                    if (exponent < 0L) {
                        exponent_negative = 1L;
                        exponent = -exponent;
                    }
                    tptr = add_int_to_tbuf(exponent, exponent_negative, base, '0', 2, tptr);
                    if (!exponent_negative) {
                        *--tptr = '+';
                    }
                    *--tptr = 'e';
                }

                if (precision > 0L) {
                    tptr = add_int_to_tbuf(remainder, 0, base, '0', precision, tptr);
                    *--tptr = '.';
                }

                tptr = add_int_to_tbuf(int_value, negative, base, pad, (pad_left != 0L) ? len : 0, tptr);

                cpy_len = strlen(tptr);
                if (cpy_len > len_left) {
                    cpy_len = len_left;
                }
                strncpy(bptr, tptr, cpy_len);
                bptr += cpy_len;
                len_left -= cpy_len;

                /* zero or space extend as needed */
                if (pad_left == 0L) {
                    int cnt = len - cpy_len;
                    while (cnt-- > 0L && (len_left != 0L)) {
                         *bptr++ = pad;
                        len_left--;
                    }
                 }
                break;
            }
#endif /* UKERNEL_FP */

            default: {
                /* unsupported format */
                const char unknown[] = "unknown";
                int32_t cpy_len = (int32_t)strlen(unknown);
                if (cpy_len > len_left) {
                    cpy_len = len_left;
                }
                strncpy(bptr, unknown, (uint32_t)cpy_len);
                bptr += cpy_len;
                len_left -= cpy_len;
                break;
              }
            }
        }
        else {
            *bptr++ = ch;
            len_left--;
        }
    }
    /* NIL terminate and return length */
    *bptr = '\0';
    return bptr - buf;
}

/*api
 * mos_vsprintf
 *
 * @brief
 * A stripped down version of vsprintf
 *
 * @param=buf - buffer to format the string into
 * @param=fmt - pointer to the format string
 * @param=args - the va_list of arguments to format
 *
 * @returns the length of the resulting string
 *
 * @desc
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
    return vsnprintf(buf, (uint32_t)INT_MAX, fmt, args);
}


/*api
 * bcm_sprintf
 *
 * @brief
 * A stripped down version of sprintf
 *
 * @param=buf - buffer to format the string into
 * @param=fmt - pointer to the format string
 *
 * @returns the length of the resulting string
 *
 * @desc
 */
int sprintf(char *buf, const char *fmt, ...)
{
    int ret;
    va_list args;

    va_start(args, fmt);
    ret = vsprintf(buf, fmt, args);
    va_end(args);
    return ret;
}

/*api
 * printf_string
 *
 * @brief
 * Wrapper to print string directly to uart
 *
 * @param=buf - pointr to string
 *
 * @returns the number of bytes sent, or 0 on failure
 *
 * @desc
 */
int32_t printf_string(const char *buf)
{
    return uart_print_string(buf);
}

/*api
 * printf
 *
 * @brief
 * A stripped down version of printf
 *
 * @param=fmt - pointer to the format string
 *
 * @returns the number of characters printed
 *
 * @desc
 */
int printf(const char *fmt, ...)
{
    int ret;
    va_list args;
    char printf_buf[PRINTF_BUF_SIZE];

    va_start(args, fmt);
    ret = vsprintf(printf_buf, fmt, args);
    (void)uart_print_string(printf_buf);
    va_end(args);
    return ret;
}

/*api
 * bcm_buf_printf
 *
 * @brief
 * A printf version to log to buffer
 *
 * @param=fmt - pointer to the format string
 *
 * @returns the number of characters stored in buffer
 *
 * @desc
 */
int buf_printf(const char *fmt, ...)
{
    int ret;
    va_list args;
    char printf_buf[PRINTF_BUF_SIZE];

    va_start(args, fmt);
    ret = vsprintf(printf_buf, fmt, args);
    logbuf_write(printf_buf, (uint32_t)(ret + 1L));
    va_end(args);
    return ret;
}
