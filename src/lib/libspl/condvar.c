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
#include <spl/condvar.h>
#include <spl/time.h>
#include <spl/cmn_err.h>
#include <spl/debug.h>
#include <string.h>
#include <errno.h>

// cv_wait() suspends the calling thread and exits the mutex atomically so
// that  another  thread which holds the mutex cannot signal on the condi-
// tion variable until the blocking thread is blocked.  Before  returning,
// the mutex is reacquired.
void cv_wait(kcondvar_t * cv, kmutex_t * m)
{
    // VERIFY that pthread_cond_wait succeeds since we are returning void.
    VERIFY0(pthread_cond_wait(&cv->cond, &m->mutex));

    // Update the mutex holder since we now own the lock.
    m->holder = pthread_to_kthread(pthread_self());
}

// Similar to cv_wait(), except that it returns -1 without the condition
// being signaled after the timeout time has been reached.
//
// timeout     A time, in absolute ticks since boot, when cv_timedwait()
//             should return.
clock_t cv_timedwait(kcondvar_t * cv, kmutex_t * m, clock_t timeout)
{
    struct timespec abstime;

    // All the callers of cv_timedwait calculate the timeout by adding
    // ticks to ddi_get_lbolt(), which is in usec.
    abstime.tv_sec = USEC_TO_SEC(timeout);
    abstime.tv_nsec = USEC_TO_NSEC(timeout - SEC_TO_USEC(abstime.tv_sec));

    int error = pthread_cond_timedwait(&cv->cond, &m->mutex, &abstime);
    switch (error) {
    case 0:
        // Update the mutex holder since we now own the lock.
        m->holder = pthread_to_kthread(pthread_self());

        // Return >0 on signalled wakeup.
        return 1;
    case ETIMEDOUT:
        // Return -1 on timeout.
        return -1;
    default:
        // And this only happens on a programmer error.
        panic("pthread_cond_timedwait() failed: %s (%d)",
                strerror(error), error);
    }
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
