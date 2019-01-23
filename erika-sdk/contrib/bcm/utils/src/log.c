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

#include "bcm/utils/inc/log.h"

#define LOG_MSG_ALIGN       4
#define LOG_BUF_LEN			(4*1024)

#define LOG_HEAD_SIZE		(sizeof(struct logbuf_header))
#define LOG_ENTRY_SIZE(x)	(LOG_HEAD_SIZE + (x))
#define LOG_MSG_OFFSET(x)	(((char*)(x)) + LOG_HEAD_SIZE)
#define MAX(x,y)			((x) ^ (((x) ^ (y)) & (-((x) < (y)))))

static uint32_t log_level = LOG_LEVEL_CRIT;
static uint32_t log_mode = 0;

#ifdef __ENABLE_LOG_BUFFER__

static SemType log_sem = STATICSEM(0);

static uint8_t log_buf[LOG_BUF_LEN];

static uint32_t log_head_off;
static uint32_t log_tail_off;

static uint32_t log_head_idx;
static uint32_t log_tail_idx;

struct logbuf_header{
	uint32_t	len;
	uint32_t	msg_len;
};

#endif /* __ENABLE_LOG_BUFFER__ */

void log_init(void)
{
	/* Log output type */
#if defined(__ENABLE_LOG_CONSOLE__)
	log_mode = LOG_MODE_CONSOLE;
#elif defined(__ENABLE_LOG_BUFFER__)
	log_mode = LOG_MODE_BUFFER;
#endif

	/* Log level */
#if defined(__ENABLE_LOG_LEVEL_CRIT__)
	log_level = LOG_LEVEL_CRIT;
#elif defined(__ENABLE_LOG_LEVEL_ERROR__)
	log_level = LOG_LEVEL_ERROR;
#elif defined(__ENABLE_LOG_LEVEL_WARN__)
	log_level = LOG_LEVEL_WARN;
#elif defined(__ENABLE_LOG_LEVEL_INFO__)
	log_level = LOG_LEVEL_INFO;
#elif defined(__ENABLE_LOG_LEVEL_DEBUG__)
	log_level = LOG_LEVEL_DEBUG;
#elif defined(__ENABLE_LOG_LEVEL_VERBOSE__)
	log_level = LOG_LEVEL_VERBOSE;
#endif

}

uint32_t get_log_level(void)
{
	return log_level;
}

void set_log_level(uint32_t level)
{
	if(level <= LOG_LEVEL_VERBOSE)
		log_level = level;
}

uint32_t get_log_mode(void)
{
	return log_mode;
}

void set_log_mode(uint32_t mode)
{
	if(mode <= LOG_MODE_BUFFER)
		log_mode = mode;
}

#ifdef __ENABLE_LOG_BUFFER__

static uint32_t log_record_size(uint32_t msg_len)
{
    uint32_t size;

    size = LOG_HEAD_SIZE + msg_len;
    size += (-size) & (LOG_MSG_ALIGN - 1);

    return size;
}

static uint32_t logbuf_has_space(uint32_t entry_size)
{
	uint32_t free_space;
	uint32_t buf_empty = log_head_idx < log_tail_idx ? 0 : 1;

	if ((log_tail_off > log_head_off) || buf_empty)
		free_space = MAX(LOG_BUF_LEN - log_tail_off, log_head_off);
	else
		free_space = log_head_off - log_tail_off;

	return (free_space >= (entry_size + LOG_HEAD_SIZE));
}

static void logbuf_get_space(uint32_t entry_size)
{
	while(!logbuf_has_space(entry_size)) {
		struct logbuf_header *hdr;

		/* Discard the oldest entry to create space
		 * for newest entry. Wrap circular buffer.
		 */
		hdr = (struct logbuf_header*)(log_buf + log_head_off);

		if(!hdr->len) {
			hdr = (struct logbuf_header*)log_buf;
			log_head_off = 0;
		} else {
			log_head_off += hdr->len;
	        log_head_idx++;
		}
	}
}

void logbuf_write(const char* msg, uint32_t msg_len)
{
	struct logbuf_header *hdr;
	uint32_t entry_size = log_record_size(msg_len);

	WaitSem(&log_sem);

    logbuf_get_space(entry_size);

	if ((log_tail_off + entry_size + LOG_HEAD_SIZE) > LOG_BUF_LEN) {
		/* Add an empty header to mark wrap of circular buffer */
		(void)memset(log_buf + log_tail_off, 0, LOG_HEAD_SIZE);
		log_tail_off = 0;
	}

	hdr = (struct logbuf_header*)(log_buf + log_tail_off);
	memcpy(LOG_MSG_OFFSET(hdr), msg, msg_len);
	hdr->msg_len = msg_len;
	hdr->len = entry_size;

	/* Get ready for next entry */
	log_tail_off += entry_size;
    log_tail_idx++;

    PostSem(&log_sem);
}

void logbuf_dump(void)
{
	uint32_t entry_head_idx, entry_head_off;

	WaitSem(&log_sem);
	entry_head_idx = log_head_idx;
	entry_head_off = log_head_off;

	while(entry_head_idx < log_tail_idx) {
		struct logbuf_header* hdr;

		hdr = (struct logbuf_header*)(log_buf + entry_head_off);
		if (!hdr->len) {
			/* Empty header, wrap the circular buf */
			hdr = (struct logbuf_header*)log_buf;
			entry_head_off = 0;
		}
		(void)printf("%s", LOG_MSG_OFFSET(hdr));
		entry_head_off += hdr->len;
		entry_head_idx++;
	}

	PostSem(&log_sem);
}

void logbuf_clear(void)
{
	WaitSem(&log_sem);
	log_head_off = log_tail_off = 0;
	log_head_idx = log_tail_idx = 0;
	PostSem(&log_sem);
}

#endif /* __ENABLE_LOG_BUFFER__ */


