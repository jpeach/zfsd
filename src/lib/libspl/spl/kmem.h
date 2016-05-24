/** @file
 *
 *  A brief file description
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef KMEM_H_AC12D9A8_4E98_4A91_8FCF_46DEA2176089
#define KMEM_H_AC12D9A8_4E98_4A91_8FCF_46DEA2176089

#include <stdlib.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    KM_SLEEP        = 0x0000,   /* can block for memory; success guaranteed */
    KM_NOSLEEP      = 0x0001,   /* cannot block for memory; may fail */
    KM_PANIC        = 0x0002,   /* if memory cannot be allocated, panic */
    KM_PUSHPAGE     = 0x0004,   /* can block for memory; may use reserve */
    KM_NORMALPRI    = 0x0008,   /* with KM_NOSLEEP, lower priority allocation */
    KM_VMFLAGS      = 0x00ff,   /* flags that must match VM_* flags */

    KM_FLAGS        = 0xffff    /* all settable kmem flags */
} kmflags_t;

typedef enum kmem_cbrc {
        KMEM_CBRC_YES,
        KMEM_CBRC_NO,
        KMEM_CBRC_LATER,
        KMEM_CBRC_DONT_NEED,
        KMEM_CBRC_DONT_KNOW
} kmem_cbrc_t;

#define kmem_alloc(size, flags)     malloc(size)
#define kmem_zalloc(size, flags)    calloc(1, size)
#define kmem_free(ptr, size)        free(ptr)
#define strfree(ptr)                free(ptr)

#define kmem_asprintf(fmt, ...) ({ \
    char * ptr; \
    asprintf(&ptr, fmt, __VA_ARGS__); \
    ptr; \
})

#define kmem_vasprintf(fmt, ap) ({ \
    char * ptr; \
    vasprintf(&ptr, fmt, ap); \
    ptr; \
})

/* vmem arenas are only every passed into kmem_cache_create, so we can
 * basically ignore them.
 */
typedef struct vmem vmem_t;

typedef struct kmem_cache kmem_cache_t;

typedef int (*kmem_constructor_t)(void *buf, void *un, unsigned kmflags);
typedef void (*kmem_destructor_t)(void *buf, void *un);
typedef void (*kmem_reclaim_t)(void *buf, void *un);
typedef kmem_cbrc_t (*kmem_move_t)(void *, void *, size_t, void *);

/*
 * Flags for kmem_cache_create()
 */
#define KMC_NOTOUCH     0x00010000
#define KMC_NODEBUG     0x00020000
#define KMC_NOMAGAZINE  0x00040000
#define KMC_NOHASH      0x00080000
#define KMC_QCACHE      0x00100000
#define KMC_KMEM_ALLOC  0x00200000      /* internal use only */
#define KMC_IDENTIFIER  0x00400000      /* internal use only */
#define KMC_PREFILL     0x00800000

kmem_cache_t *kmem_cache_create(const char *name, size_t bufsize,
     size_t align, kmem_constructor_t, kmem_destructor_t, kmem_reclaim_t,
     void *arg, vmem_t *vmp, unsigned cflags);

void    kmem_cache_destroy(kmem_cache_t *cp);
void *  kmem_cache_alloc(kmem_cache_t *cp, unsigned kmflag);
void    kmem_cache_free(kmem_cache_t *cp, void *ptr);

void	kmem_cache_set_move(kmem_cache_t *cp, kmem_move_t mv);
void 	kmem_cache_reap_now(kmem_cache_t *cp);

static inline bool
kmem_debugging() {
    return false;
}

/*
 * Public segment types
 */
#define VMEM_ALLOC      0x01
#define VMEM_FREE       0x02

extern vmem_t *heap_arena;     /* primary kernel heap arena */

size_t vmem_size(vmem_t *vmp, int typemask);

static inline void
vmem_qcache_reap(vmem_t *vmp) {
}

// Pages to bytes.
static inline size_t
ptob(unsigned pages) {
    return pages * getpagesize();
}

#define PAGESIZE getpagesize()
#define physmem sysconf(_SC_PHYS_PAGES)

#ifdef  __cplusplus
}
#endif

#endif /* KMEM_H_AC12D9A8_4E98_4A91_8FCF_46DEA2176089 */
