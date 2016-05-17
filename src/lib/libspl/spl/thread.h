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

#ifndef THREAD_H_94F3AC58_A4AE_41B9_8BE8_9A010FAB68DB
#define THREAD_H_94F3AC58_A4AE_41B9_8BE8_9A010FAB68DB

#include <pthread.h>

#ifdef  __cplusplus
extern "C" {
#endif

// A kthread is an opaque struct aliased to a pthread_t.
typedef struct kthread kthread_t;

// 0 is a valid pthread_t (pthread_self() returns it for the main
// thread), so define a well-known invalid type.
#define INVALID_KTHREAD ((kthread_t *)(uintptr_t)(-1))

typedef void * (*pthread_proc_t)(void *);

static inline kthread_t *
pthread_to_kthread(pthread_t threadid) {
    return (kthread_t *)((uintptr_t)threadid);
}

static inline pthread_t
kthread_to_pthread(kthread_t * threadid) {
    return (pthread_t)((uintptr_t)threadid);
}

#define curthread ({ pthread_to_kthread(pthread_self()); })

#define thread_create(stk, stksize, proc, arg, len, pp, state, pri) \
    thread_create_ex(proc, arg, #proc)

kthread_t * thread_create_ex(pthread_proc_t proc, void *arg, const char * name);
void thread_join(kthread_t * thr);
void thread_exit(void);

void thread_setname(kthread_t * thr, const char * name);
pid_t thread_gettid();

#ifdef  __cplusplus
}
#endif

#endif /* THREAD_H_94F3AC58_A4AE_41B9_8BE8_9A010FAB68DB */
