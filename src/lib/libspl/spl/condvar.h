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

#ifndef CONDVAR_H_2A7129A3_8BA3_446C_B60D_3E483FBBCA50
#define CONDVAR_H_2A7129A3_8BA3_446C_B60D_3E483FBBCA50

#include <spl/thread.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    CV_DEFAULT,
} kcv_type_t;

typedef struct kcondvar {
    pthread_cond_t cond;
} kcondvar_t;

static inline void
cv_init(kcondvar_t * cv, char * name, kcv_type_t type, void * arg) {
    pthread_cond_init(&cv->cond, NULL);
}

static inline void
cv_destroy(kcondvar_t * cv) {
    pthread_cond_destroy(&cv->cond);
}

static inline void
cv_signal(kcondvar_t * cv) {
    pthread_cond_signal(&cv->cond);
}

static inline void
cv_broadcast(kcondvar_t * cv) {
    pthread_cond_broadcast(&cv->cond);
}

void cv_wait(kcondvar_t * cv, kmutex_t * m);
clock_t cv_timedwait(kcondvar_t * cv, kmutex_t * m, clock_t timeout);

#ifdef  __cplusplus
}
#endif

#endif /* CONDVAR_H_2A7129A3_8BA3_446C_B60D_3E483FBBCA50 */
