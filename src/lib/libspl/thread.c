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
#include <spl/thread.h>

void
thread_setname(kthread_t * thr, const char * name)
{
    if (name) {
        pthread_setname_np(kthread_to_pthread(thr), name);
    }
}

kthread_t *
thread_create_ex(pthread_proc_t proc, void *arg, const char * name)
{
    pthread_t tid;

    if (pthread_create(&tid, NULL, proc, arg) == 0) {
        thread_setname(pthread_to_kthread(tid), name);
        return pthread_to_kthread(tid);
    }

    return NULL;
}

void
thread_exit(void)
{
    pthread_exit(NULL);
}

void
thread_join(kthread_t * thr)
{
    pthread_join(kthread_to_pthread(thr), NULL);
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
