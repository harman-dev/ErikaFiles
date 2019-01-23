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
#include <stdint.h>
#include "bcm/utils/inc/malloc.h"
#include "bcm/utils/inc/debug.h"

/* Macros */
#define MSIZE_ALIGN 	32UL
#define MALLOC_MAGIC_ALLOCATED 0x4d6d4163UL       /* "MmAc" */
#define MALLOC_MAGIC_FREE      0x4d6d4627UL       /* "MmFr" */

#define ALIGN(x) (((x) + MSIZE_ALIGN - 1UL) & (~(MSIZE_ALIGN - 1UL)))

#define MALLOC_ALLOC_END(m) (((char *) (m)) + (m)->size)
/* Macro to convert allocated magic to free magic */
#define MALLOC_ALLOC_MAGIC(free) ((free) ^ MALLOC_MAGIC_ALLOCATED ^ MALLOC_MAGIC_FREE)

extern uint8_t system_malloc_baseaddr[];
extern uint8_t system_malloc_endaddr[];

extern uint8_t eth_malloc_baseaddr[];
extern uint8_t eth_malloc_endaddr[];

static uint32_t cur_heap_magic = 0;

typedef struct malloc_free_s {
	uint32_t magic;
	uint32_t size;
	struct malloc_free_s *next;
	uint32_t fill;
} malloc_free_t;

typedef struct {
	uint32_t magic;
	uint32_t size;
} malloc_alloc_t;

static malloc_free_t *default_heap;
static malloc_free_t *eth_heap;
void mutex_lock(void)
{
	StatusType status;

	status = GetResource(MallocLock);
	if (status != E_OK)
		LOG_ERROR("GetResource failed!!! Status(%d)\n", status);
}

void mutex_unlock(void)
{
	StatusType status;

	status = ReleaseResource(MallocLock);
	if (status != E_OK)
		LOG_ERROR("ReleaseResource failed!!! status(%d)\n", status);
}

static void *malloc_internal(malloc_free_t **fp, uint32_t size)
{
    malloc_alloc_t *alloc = NULL;
    uint32_t free_magic;
    uint32_t size_loc = size;
    malloc_free_t **fp_loc = fp;

    if ((fp_loc == NULL) || (size_loc == 0UL)) {      /* If no malloc area or len */
        return (NULL);
    }

    free_magic = (*fp_loc)->magic;
    size_loc = ALIGN(size_loc + sizeof(malloc_alloc_t));

    /* Search for big enough free area */
    while ((*fp_loc != NULL) && ((*fp_loc)->size < size_loc)) {
	    DBG_ASSERT((*fp_loc)->magic == free_magic, "Malloc curruption");
	    fp_loc = &((*fp_loc)->next);
    }

    if (*fp_loc == NULL) {      /* If no chunk big enough */
	    alloc = NULL;

    } else {
	    DBG_ASSERT((*fp_loc)->magic == free_magic, "Malloc curruption");
	    if (size_loc == (*fp_loc)->size) {
		    /* Exact fit - remove free element */
		    alloc = (malloc_alloc_t *) *fp_loc;
		    *fp_loc = (*fp_loc)->next;

	    } else if (size + sizeof(malloc_free_t) == (*fp_loc)->size) {
		    /* Only the next header would be left */
		    alloc = (malloc_alloc_t *) *fp_loc;
		    *fp_loc = (*fp_loc)->next;
		    size_loc += sizeof(malloc_free_t);

	    } else {
		    /* Carve free area into 2.  Return second part. */
		    (*fp_loc)->size -= size_loc;
		    alloc = (malloc_alloc_t *) (((char *) (*fp_loc)) + (*fp_loc)->size);
	    }
    }

    if (alloc == NULL) {
        return (NULL);
    }

    alloc->magic = MALLOC_ALLOC_MAGIC((*fp_loc)->magic);
    alloc->size = size_loc;

    return ((void *)&alloc[1]);
}

static void free_internal(malloc_free_t **fp, const void * const free)
{
    malloc_free_t *np;
    malloc_free_t **fp_loc = fp;

    malloc_alloc_t *alloc = (malloc_alloc_t *) (((char *) free) - sizeof(malloc_alloc_t));
    uint32_t free_magic = (*fp_loc)->magic;

    DBG_ASSERT(alloc->magic == MALLOC_ALLOC_MAGIC(free_magic), "Malloc corruption");

    /*N.B.: This could be a slow search if there is a lot of fragmentation */
    while (((*fp_loc)->next != NULL) && (((void *) (*fp_loc)->next) < ((void *)alloc))) {
	    fp_loc = &((*fp_loc)->next);
    }

    DBG_ASSERT((*fp_loc)->magic == free_magic, "Malloc pool curruption");

    if (MALLOC_ALLOC_END(*fp_loc) == (char *) alloc) {
	    /* Allocation is contiguous - merge with free */
	    (*fp_loc)->size += alloc->size;
	    np = *fp_loc;

    } else {
	    /* Set up free area */
	    uint32_t size = alloc->size;        /* Save for safety */
	    np = (malloc_free_t *) alloc;
	    np->size = size;
	    np->magic = free_magic;
	    np->next = (*fp_loc)->next;
	    (*fp_loc)->next = np;
    }

    /* Check for a merge with the following free area */
    if (MALLOC_ALLOC_END(np) == ((char *) (np->next))) {
	    DBG_ASSERT(np->next->magic == free_magic, "Malloc pool curruption");
	    np->size += np->next->size;
	    np->next = np->next->next;
    }

}

void *malloc(size_t size)
{
    void *ret;

    mutex_lock();
    ret = malloc_internal(&default_heap, size);
    mutex_unlock();

    return ret;
}

void *eth_malloc(uint32_t size)
{
    void *ret;

    mutex_lock();
    ret =  malloc_internal(&eth_heap, size);
    mutex_unlock();

    return ret;
}

void free(void *ptr)
{
    mutex_lock();
    free_internal(&default_heap, ptr);
    mutex_unlock();
}

void eth_free(void *ptr)
{
    mutex_lock();
    free_internal(&eth_heap, ptr);
    mutex_unlock();
}


void *malloc_atomic(uint32_t size)
{
    void *ret;

    SuspendAllInterrupts();
    ret = malloc_internal(&default_heap, size);
    ResumeAllInterrupts();

    return ret;
}

void *eth_malloc_atomic(uint32_t size)
{
    void *ret;

    SuspendAllInterrupts();
    ret =  malloc_internal(&eth_heap, size);
    ResumeAllInterrupts();

    return ret;
}

void free_atomic(void *ptr)
{
    SuspendAllInterrupts();
    free_internal(&default_heap, ptr);
    ResumeAllInterrupts();
}

void eth_free_atomic(void *ptr)
{
    SuspendAllInterrupts();
    free_internal(&eth_heap, ptr);
    ResumeAllInterrupts();
}


malloc_free_t * heap_init(void *base, uint32_t size)
{
    malloc_free_t *init = (malloc_free_t *) base;
    init->next = NULL;
    init->size = size;
    init->magic = MALLOC_MAGIC_FREE ^ cur_heap_magic++;
    return (init);
}

void mm_init(void)
{
    uint32_t sys_heap_size = (uint32_t)(system_malloc_endaddr - system_malloc_baseaddr);
    uint32_t eth_heap_size = (uint32_t)(eth_malloc_endaddr - eth_malloc_baseaddr);

    default_heap = heap_init(system_malloc_baseaddr, sys_heap_size);

    eth_heap = heap_init(eth_malloc_baseaddr, eth_heap_size);
}
