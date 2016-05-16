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

#include <sys/kmem.h>
#include <spl/debug.h>

struct vmem
{
};

struct kmem_cache
{
    size_t              km_objsize;
    kmem_constructor_t  km_init;
    kmem_destructor_t   km_fini;
    void *              km_arg;
    unsigned            km_count;
};

vmem_t * zio_arena;

static void
kmem_cache_object_init(kmem_cache_t *cp, void * ptr, unsigned kmflags)
{
    if (cp->km_init) {
        cp->km_init(ptr, cp->km_arg, kmflags);
    }
}

static void
kmem_cache_object_fini(kmem_cache_t *cp, void * ptr)
{
    if (cp->km_fini) {
        cp->km_fini(ptr, cp->km_arg);
    }
}

kmem_cache_t *
kmem_cache_create(const char *name, size_t bufsize, size_t align,
     kmem_constructor_t constructor, kmem_destructor_t destructor,
     kmem_reclaim_t reclaim, void *arg, vmem_t *vmp, unsigned cflags)
{
    kmem_cache_t * cache;

    ASSERT(vmp == NULL);

    if (reclaim) {
        spl_dprintf("ignoring unimplemented reclaim function %p\n", reclaim);
    }

    cache = kmem_zalloc(sizeof(struct kmem_cache), KM_SLEEP);
    cache->km_objsize = bufsize;
    cache->km_init = constructor;
    cache->km_fini = destructor;
    cache->km_arg = arg;

    return cache;
}

void
kmem_cache_destroy(kmem_cache_t *cp)
{
    // Clients are required to destroy outstanding objects before
    // destroying the pool.
    ASSERT(cp->km_count == 0);
    kmem_free(cp, sizeof(*cp));
}

void *
kmem_cache_alloc(kmem_cache_t *cp, unsigned kmflags)
{
    void * ptr;

    ptr = kmem_alloc(cp->km_objsize, kmflags);
    if (ptr) {
        kmem_cache_object_init(cp, ptr, kmflags);
        cp->km_count++;
    }

    return ptr;
}

void
kmem_cache_free(kmem_cache_t *cp, void *ptr)
{
    if (ptr) {
        kmem_cache_object_fini(cp, ptr);
        cp->km_count--;
    }
}

void
kmem_cache_set_move(kmem_cache_t *cp, kmem_move_t mv)
{
    (void)cp;
    (void)mv;

    // Do nothing for now. The move callback is used during cache
    // compaction, which we don't do ...
}

void
kmem_cache_reap_now(kmem_cache_t *cp)
{
    (void)cp;

    // Do nothing for now, since kmem_cache is just a malloc wrapper.
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
