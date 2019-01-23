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

#include <assert.h>
#include <stdlib.h>
#include "bcm/utils/inc/shell.h"
#include "bcm/drivers/inc/uart.h"
#include "bcm/utils/inc/print.h"
#include "bcm/utils/inc/build_info.h"
#include "mcu/bcm/common/inc/bcm_info.h"
#include "bcm/drivers/inc/flash.h"

#define HISTORY_BUFFER_LENGTH 	10
#define CMD_BUFFER_LENGTH 		128UL

char history_buffer[HISTORY_BUFFER_LENGTH][CMD_BUFFER_LENGTH];
uint8_t current_history_buffer_index=0;

static void shell_loglevel(int argc, char *argv[]);
static void shell_logmode(int argc, char *argv[]);
static void shell_logdump(int argc, char *argv[]);
static void shell_help(int argc, char *argv[]);
static void shell_buildinfo(int argc, char *argv[]);
static void shell_chipinfo(int argc, char *argv[]);
static void shell_os_version(int argc, char *argv[]);
static void shell_flash(int argc, char *argv[]);
void shell_eth(int argc, char *argv[]) __attribute__ ((weak));
void shell_etg(int argc, char *argv[]) __attribute__ ((weak));
#ifdef __ENABLE_AVB__
void shell_avb(int argc, char *argv[]);
#endif

#ifdef __ENABLE_ACD__
void shell_acd(int argc, char *argv[]);
#endif

#ifdef __ENABLE_AUTOTEST__
void shell_autotest(int argc, char *argv[]);
#endif

#ifdef __ENABLE_DRIVERTEST__
void shell_bcmdriver(int argc, char *argv[]);
#endif
struct {
    const char    *cmd;
    void    (*func)(int argc, char *argv[]);
    const char    *help;
} shell_cmds[] = {
        { "loglevel", shell_loglevel, "Enable logging. Valid values are "
                                "crit|error|warn|info|debug|verbose" },
        { "logmode", shell_logmode, "Select logging output. Valid values are "
                                    "console|buffer" },
        { "logdump", shell_logdump, "Dump log buffer to console"},
#ifdef __ENABLE_AVB__
        { "avb", shell_avb, "AVB Commands"},
#endif
#ifdef __ENABLE_ACD__
        { "acd", shell_acd, "ACD Command"},
#endif
        { "etg", shell_etg, "Ethernet traffic generator control" },
        { "eth", shell_eth, "[dump|mibs]" },
#ifdef __ENABLE_DRIVERTEST__
		{ "drivertest", shell_bcmdriver, "Command interface for BCM Driver Testing" },
#endif
        { "help", shell_help, "list commands" },
        { "os_version", shell_os_version, "OS version info" },
		{ "buildinfo", shell_buildinfo, "Prints build info of the software" },
		{ "chipinfo", shell_chipinfo, "Prints chip info" },
#ifdef __ENABLE_AUTOTEST__
		{ "autotest", shell_autotest, "Autotest control" },
#endif
#ifdef __ENABLE_FLASH_DRV__
        {"flash", shell_flash, "Flash commands(for debug purpose only)"},
#endif
        { NULL, NULL }
};


/*@api
 *  shell_loglevel
 * @brief
 *  Prints trace logs onto the console.
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */
static void shell_loglevel(int argc, char *argv[])
{
    const char* log_string = NULL;

    if (argc == 1) {
        switch(get_log_level()) {
            case LOG_LEVEL_CRIT:
                log_string = "crit";
                break;
            case LOG_LEVEL_ERROR:
                log_string = "error";
                break;
            case LOG_LEVEL_WARN:
                log_string = "warn";
                break;
            case LOG_LEVEL_INFO:
                log_string = "info";
                break;
            case LOG_LEVEL_DEBUG:
                log_string = "debug";
                break;
            case LOG_LEVEL_VERBOSE:
                log_string = "verbose";
                break;
        }
        (void)printf("Current log level is %s\n", log_string);
        return;
    }

    if (argc != 2) {
        (void)printf("Usage: loglevel crit|error|warn|info|debug|verbose\n");
        return;
    }

    if (strncmp(argv[1], "crit", CMD_BUFFER_LENGTH) == 0) {
        set_log_level(LOG_LEVEL_CRIT);
    } else if (strncmp(argv[1], "error", CMD_BUFFER_LENGTH) == 0) {
        set_log_level(LOG_LEVEL_ERROR);
    } else if (strncmp(argv[1], "warn", CMD_BUFFER_LENGTH) == 0) {
        set_log_level(LOG_LEVEL_WARN);
    } else if (strncmp(argv[1], "info", CMD_BUFFER_LENGTH) == 0) {
        set_log_level(LOG_LEVEL_INFO);
    } else if (strncmp(argv[1], "debug", CMD_BUFFER_LENGTH) == 0) {
        set_log_level(LOG_LEVEL_DEBUG);
    } else if (strncmp(argv[1], "verbose", CMD_BUFFER_LENGTH) == 0) {
        set_log_level(LOG_LEVEL_VERBOSE);
    } else {
        (void)printf("%s is invalid value\n", argv[1]);
    }
    return;
}

/*@api
 *  shell_logmode
 * @brief
 *  Prints trace logs onto the console.
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */
static void shell_logmode(int argc, char *argv[])
{
    const char* log_string = NULL;
    if (argc == 1) {
        switch(get_log_mode()) {
            case LOG_MODE_CONSOLE:
                log_string = "console";
                break;
            case LOG_MODE_BUFFER:
                log_string = "buffer";
                break;
        }
        (void)printf("Current log type is %s\n", log_string);
        return;
    }

    if (argc != 2) {
        (void)printf("Usage: logtype console|buffer\n");
        return;
    }

    if (strncmp(argv[1], "console", CMD_BUFFER_LENGTH) == 0) {
        set_log_mode(LOG_MODE_CONSOLE);
    } else if (strncmp(argv[1], "buffer", CMD_BUFFER_LENGTH) == 0) {
        set_log_mode(LOG_MODE_BUFFER);
    } else {
        (void)printf("%s is invalid value\n", argv[1]);
    }
    return;
}


/*@api
 *  shell_dump
 * @brief
 *  Prints log buffer to console
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */
static void shell_logdump(int argc, char *argv[])
{
	logbuf_dump();
}

#ifdef __ENABLE_FLASH_DRV__
/**
 * Flash related command in shell is added for debug
 * purpose only.
 *
 * Flash write command does not erase and write
 * rather it simply tries to write a given value to a
 * given location in flash (which means it may be able to
 * flip the bit from 1 to 0 but not from 0 to 1.)
 *
 * Flash read command, reads the flash at given
 * location and prints in the shell. User can
 * only read upto 32 bytes in 1 read command
 */
static void shell_flash(int argc, char *argv[])
{
    int err = ERR_OK;
    char byteData;
    uint32_t len;
    uint32_t addr;
    uint32_t i;
    char buf[32];
    flash_dev_t *dev = flash_get_dev();

    if (dev == NULL) {
        printf("invalid flash device \n");
        return;
    }

    if (strncmp(argv[1], "write", CMD_BUFFER_LENGTH) == 0) {
        if (argc != 4UL) {
            printf("Usage: flash write <address> <byte>");
            return;
        }
        byteData = strtol(argv[3], NULL, 16);
        addr = strtol(argv[2], NULL, 16);
        printf("addr: %x bytedata: %x\n", addr, byteData);
        err = flash_write(dev, addr, &byteData, 1UL);
        if (err != ERR_OK) {
            printf("flash_write failed. err: %d\n", err);
        }
    }

    if (strncmp(argv[1], "read", CMD_BUFFER_LENGTH) == 0) {
        if (argc != 4UL) {
            printf("Usage: flash read <address (0x..)> <count (0x..)>");
            return;
        }
        len = strtol(argv[3], NULL, 16);
        addr = strtol(argv[2], NULL, 16);
        if ((len == 0UL) || (len > 32UL)) {
            printf("len is 0 or > 32 bytes\n");
            return;
        }
        err = flash_read(dev, addr, buf, len);
        if (err != ERR_OK) {
            printf("flash_read failed. err: %d\n", err);
            return;
        }
        for (i = 0UL; i < len; i++) {
            if ((i % 4) == 0UL) {
                printf("\n");
            }
            printf("0x%02x ", buf[i]);
        }
    }
}
#endif

/*@api
 *  shell_help
 * @brief
 *  Print the shell help page.
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */
static void shell_help(int argc, char *argv[])
{
    int i;

    (void)printf("Help:\n");
    for (i = 0; (shell_cmds[i].cmd != NULL); ++i) {
        if (shell_cmds[i].func != NULL) {
            (void)printf("\t%10s\t%s\n", shell_cmds[i].cmd, shell_cmds[i].help);
        }
    }
#ifdef PROFILER
    if (mos_profile_data) {
        (void)printf("profile data: writefile ,raw \"data.raw\" =%p..+%d\n",
                   mos_profile_data, mos_profile_len);
        (void)printf("profile data: writefile ,ascii,long \"data.ascii\" =%p..+%d\n",
                   mos_profile_data, mos_profile_len);
    }
    (void)printf("core-dump:\n");
    (void)printf("    writefile ,ascii,long \"image-itcm.ascii\" =%p..+%d\n",
               itcm_baseaddr, (itcm_endaddr - itcm_baseaddr));
    (void)printf("    writefile ,ascii,long \"image-dtcm.ascii\" =%p..+%d\n",
               dtcm_baseaddr, (dtcm_endaddr - dtcm_baseaddr));
    (void)printf("    writefile ,ascii,long \"image-system.ascii\" =%p..+%d\n",
               system_baseaddr, (system_endaddr - system_baseaddr));
    (void)printf("    writefile ,ascii,long \"image-uncached.ascii\" =%p..+%d\n",
               uncached_baseaddr, (uncached_endaddr - uncached_baseaddr));
#endif
}


/*@api
 *  shell_buildinfo
 * @brief
 *  Prints build info of the software.
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */
static void shell_buildinfo(int argc, char *argv[])
{
	build_info_full();
	return;
}

/*@api
 *  shell_os_version
 * @brief
 *  Prints os version of the software.
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */

static void shell_os_version(int argc, char *argv[])
{
    (void)printf("OS Version: "OS_IMAGE_VERSION"\n");
}

/*@api
 *  shell_chipinfo
 * @brief
 *  Prints chip info
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 * @returns none
 */
static void shell_chipinfo(int argc, char *argv[])
{
	mcu_info();
	return;
}

/*
 * @api
 * shell_splitline
 *
 * @brief
 * Split a command line into argc/argv
 *
 * @param=buffer - line to split
 * @param=argc - location to return number of arguments
 * @param=argv - argument array
 * @param=len - length of argument array
 *
 * @desc
 * Modifies the input buffer.
 */
static void shell_splitline(char *buffer, int *argc, char *argv[], int len)
{
    char *ptr = buffer;
    uint32_t idx = 0UL;

    *argc = 0L;

    /* break up the arguments */
    while (*argc < len) {
        /* consume leading whitespace */
        while (ptr[idx] == ' ') {
            idx++;
        }
        /* Check for end of line */
        if (ptr[idx] == 0) {
            break;
        }
        argv[(*argc)++] = &ptr[idx];
        /* Scan past the argument */
        while ((ptr[idx] != 0) && (ptr[idx] != ' ')) {
            ++idx;
        }
        /* Check for end of line */
        if (ptr[idx] == 0) {
            break;
        }
        /* Null-terminate the argument */
        ptr[idx] = '\0';
        idx++;
    }
}

/*
 * @api
 * shell_readline
 *
 * @brief
 * Read a line from the console.
 *
 * @param=buffer - buffer to store characters into.
 * @param=len - buffer length
 * @returns - none
 */
static void shell_readline(char *buffer, uint32_t len)
{
    uint32_t idx = 0UL;
    buffer[idx] = '\0';
    (void)printf("=> ");

    while (1) {
        char ch;
        if (uart_getc((uint8_t *)&ch) != 0L) {
            continue;
        }
        /* Check for newline */
        if (ch == '\r') {
            (void)printf("\n");
            return;
        }
        /* Check for backspace */
        if (ch == '\b') {
            if (idx > 0UL) {
                buffer[--idx] = '\0';
                (void)printf("\b \b");
            }
            continue;
        }
        /* Insert the character if there's room */
        if (idx < (len - 1UL)) {
            buffer[idx++] = ch;
            buffer[idx] = '\0';
            (void)printf("%c", ch);
        }
    }
}

/*@api
 *  shell_cmd
 * @brief
 *  Parse and execute a single shell command
 * @param=none
 * @returns none
 */
static void shell_cmd(char *cmd)
{
    int i;
    int argc;
    char *argv[20] = {NULL};

    shell_splitline(cmd, &argc, argv, (int32_t)(sizeof(argv)/sizeof(argv[0])));
    if (argc > 0) {
        for (i = 0; shell_cmds[i].cmd; ++i) {
            if (shell_cmds[i].func && !strncmp(argv[0], shell_cmds[i].cmd, CMD_BUFFER_LENGTH)) {
                (*shell_cmds[i].func)(argc, argv);
                return;
            }
        }
        (void)printf("unknown command '%s'\n", argv[0]);
    }
}


/*@api
 *  shell_cmd_loop
 * @brief
 *  Core shell command loop. Read and parse commands from the console
 * @param=none
 * @returns none
 */
TASK(TaskShell)
{
    char buffer[CMD_BUFFER_LENGTH] = {0};

    if (uart_initialized() == 0UL) {
        /* No UART to read from, terminate task */
        TerminateTask();
    }

    while (1) {
        shell_readline(buffer, sizeof(buffer));
        memcpy(history_buffer[current_history_buffer_index],buffer,CMD_BUFFER_LENGTH);

        current_history_buffer_index =
        (current_history_buffer_index + (uint8_t)1) % HISTORY_BUFFER_LENGTH;

        shell_cmd(buffer);
    }
}
