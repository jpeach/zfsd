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
#include <spl/mutex.h>
#include <spl/debug.h>

kmutex_t cpu_lock = {
    .holder = INVALID_KTHREAD,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

void
mutex_init(kmutex_t * m, char * name, kmutex_type_t type, void * arg)
{
    pthread_mutexattr_t attr;

    VERIFY0(pthread_mutexattr_init(&attr));

    /* Error checking mutexes will return an error if we try to recursively
     * lock or try to unlock one we have not locked.
     *
     * See pthread_mutex_lock(3p)
     */
    VERIFY0(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK));

    VERIFY0(pthread_mutex_init(&m->mutex, &attr));
    VERIFY0(pthread_mutexattr_destroy(&attr));

    m->holder = INVALID_KTHREAD;
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
