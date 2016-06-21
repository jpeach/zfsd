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

#include <spl/types.h>
#include <spl/rwlock.h>
#include <spl/thread.h>
#include <spl/cmn_err.h>
#include <spl/debug.h>
#include <ck_rwlock.h>
#include <stdalign.h>
#include <assert.h>

static_assert(sizeof(krwlock_t) > sizeof(ck_rwlock_t), "krwlock_t size mismatch");
static_assert(alignof(krwlock_t) >= alignof(ck_rwlock_t), "krwlock_t alignment mismatch");

static inline ck_rwlock_t *
krwlock_to_ck(krwlock_t * rw) {
    return (ck_rwlock_t *)(&rw->opaque);
}

void rw_init(krwlock_t * rw, const char * name, krw_type_t type, void * arg)
{
    (void)arg;
    (void)name;

    /* NOTE: Solaris ignores the type argument, and ZFS passes junk (0). */
    rw->writer = INVALID_KTHREAD;
    ck_rwlock_init((ck_rwlock_t *)rw);
}

void rw_destroy(krwlock_t * rw)
{
    ck_rwlock_t * ck = (ck_rwlock_t *)rw;

    VERIFY3(rw->writer, ==, INVALID_KTHREAD);
    VERIFY3(ck->writer, ==, 0);
    VERIFY3(ck->n_readers, ==, 0);

    memset(rw, 0, sizeof(*rw));
    rw->writer = INVALID_KTHREAD;
}

bool rw_iswriter(krwlock_t * rw)
{
    return rw->writer != INVALID_KTHREAD &&
        pthread_to_kthread(pthread_self()) == rw->writer;
}

void rw_enter(krwlock_t * rw, krw_t which)
{
    ck_rwlock_t * ck = (ck_rwlock_t *)rw;

    switch (which) {
    case RW_WRITER:
        ck_rwlock_write_lock(ck);
        rw->writer = pthread_to_kthread(pthread_self());
        break;
    case RW_READER:
        ck_rwlock_read_lock(ck);
        break;
    }
}

bool rw_tryenter(krwlock_t * rw, krw_t which)
{
    ck_rwlock_t * ck = (ck_rwlock_t *)rw;

    switch (which) {
    case RW_WRITER:
        if (ck_rwlock_write_trylock(ck)) {
            rw->writer = pthread_to_kthread(pthread_self());
            return true;
        }
        return false;
    case RW_READER:
        return ck_rwlock_read_trylock(ck);
    default:
        panic("invalid rwlock type %d", which);
    }
}

void rw_exit(krwlock_t * rw)
{
    ck_rwlock_t * ck = krwlock_to_ck(rw);

    if (rw_iswriter(rw)) {
        rw->writer = INVALID_KTHREAD;
        ck_rwlock_write_unlock(ck);
    } else {
        ck_rwlock_read_unlock(ck);
    }
}

void rw_downgrade(krwlock_t * rw)
{
    rw->writer = INVALID_KTHREAD;
    ck_rwlock_write_downgrade(krwlock_to_ck(rw));
}

bool rw_tryupgrade(krwlock_t * rw)
{
    (void)rw;
    return false;
}

bool rw_read_held(krwlock_t * rw)
{
    ck_rwlock_t * ck = (ck_rwlock_t *)rw;
    return ck_pr_load_uint(&ck->n_readers) != 0;
}

bool rw_write_held(krwlock_t * rw)
{
    ck_rwlock_t * ck = (ck_rwlock_t *)rw;
    return ck_pr_load_uint(&ck->writer) != 0;
}

bool rw_lock_held(krwlock_t * rw)
{
    return ck_rwlock_locked(krwlock_to_ck(rw));
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
