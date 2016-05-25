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

#ifndef MUTEX_H_9CE59889_51F0_4A80_9E18_51DE8DE39B33
#define MUTEX_H_9CE59889_51F0_4A80_9E18_51DE8DE39B33

#include <spl/thread.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    MUTEX_DEFAULT = 6       /* kernel default mutex */
} kmutex_type_t;

typedef struct kmutex
{
    kthread_t       *holder;
    pthread_mutex_t mutex;
} kmutex_t;

static inline void
mutex_init(kmutex_t * m, char * name, kmutex_type_t type, void * arg) {
    m->holder = INVALID_KTHREAD;
    pthread_mutex_init(&m->mutex, NULL);
}

static inline void
mutex_destroy(kmutex_t * m) {
    pthread_mutex_destroy(&m->mutex);
}

static inline void
mutex_enter(kmutex_t * m) {
    pthread_mutex_lock(&m->mutex);
    m->holder = pthread_to_kthread(pthread_self());
}

static inline bool
mutex_tryenter(kmutex_t * m) {
    if (pthread_mutex_trylock(&m->mutex) == 0) {
        m->holder = curthread;
        return true;
    }

    return false;
}

static inline void
mutex_exit(kmutex_t * m) {
    m->holder = INVALID_KTHREAD;
    pthread_mutex_unlock(&m->mutex);
}

static inline kthread_t *
mutex_owner(const kmutex_t * m) {
    return m->holder;
}

static inline bool
MUTEX_HELD(const kmutex_t *m) {
    return pthread_equal(kthread_to_pthread(m->holder), pthread_self());
}

static inline bool
MUTEX_NOT_HELD(const kmutex_t *m) {
    return !MUTEX_HELD(m);
}

#ifdef  __cplusplus
}
#endif

#endif /* MUTEX_H_9CE59889_51F0_4A80_9E18_51DE8DE39B33 */
