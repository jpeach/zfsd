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

#ifndef RWLOCK_H_1874DE7E_D450_46AB_8F78_FD84DA289150
#define RWLOCK_H_1874DE7E_D450_46AB_8F78_FD84DA289150

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct kthread kthread_t;

typedef struct krwlock {
    unsigned int opaque[2];
    kthread_t * writer;
} krwlock_t;

typedef enum {
    RW_DEFAULT = 4              /* kernel default rwlock */
} krw_type_t;

typedef enum {
    RW_WRITER,
    RW_READER,
} krw_t;

void rw_init(krwlock_t * rw, const char * name, krw_type_t type, void * arg);
void rw_destroy(krwlock_t * rw);
void rw_enter(krwlock_t * rw, krw_t which);
bool rw_tryenter(krwlock_t * rw, krw_t which);
void rw_exit(krwlock_t * rw);

void rw_downgrade(krwlock_t * rw);
bool rw_tryupgrade(krwlock_t * rw);

bool rw_read_held(krwlock_t * rw);
bool rw_write_held(krwlock_t * rw);
bool rw_lock_held(krwlock_t * rw);

#define RW_READ_HELD(lock)      (rw_read_held((lock)))
#define RW_WRITE_HELD(lock)     (rw_write_held((lock)))
#define RW_LOCK_HELD(lock)      (rw_lock_held((lock)))

#ifdef  __cplusplus
}
#endif

#endif /* RWLOCK_H_1874DE7E_D450_46AB_8F78_FD84DA289150 */
