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

#ifndef ATOMIC_H_03388ABD_5CE5_4B1C_815C_5258881CFB35
#define ATOMIC_H_03388ABD_5CE5_4B1C_815C_5258881CFB35

#include <spl/types.h>
#include <ck_pr.h>

#ifdef  __cplusplus
extern "C" {
#endif

// Atomic CAS, returning the old value.
static inline uint32_t
atomic_cas_32(uint32_t * ptr, uint32_t oldval, uint32_t newval) {
    uint32_t orig;
    ck_pr_cas_32_value(ptr, oldval, newval, &orig);
    return orig;
}

// Atomic CAS, returning the old value.
static inline uint64_t
atomic_cas_64(uint64_t * ptr, uint64_t oldval, uint64_t newval) {
    uint64_t orig;
    ck_pr_cas_64_value(ptr, oldval, newval, &orig);
    return orig;
}

static inline void
atomic_inc_32(uint32_t * ptr) {
    ck_pr_inc_32(ptr);
}

static inline void
atomic_inc_64(uint64_t * ptr) {
    ck_pr_inc_64(ptr);
}

static inline void
atomic_dec_32(uint32_t * ptr) {
    ck_pr_dec_32(ptr);
}

static inline void
atomic_dec_64(uint64_t * ptr) {
    ck_pr_dec_64(ptr);
}

static inline void
atomic_add_32(uint32_t * ptr, int32_t amt) {
    if (amt < 0) {
        ck_pr_sub_32(ptr, -amt);
    } else {
        ck_pr_add_32(ptr, amt);
    }
}

static inline void
atomic_add_64(uint64_t * ptr, int64_t amt) {
    if (amt < 0) {
        ck_pr_sub_64(ptr, -amt);
    } else {
        ck_pr_add_64(ptr, amt);
    }
}

// Atomic decrement, returning the new value.
static inline uint32_t
atomic_dec_32_nv(uint32_t * ptr) {
    return __sync_sub_and_fetch(ptr, 1);
}

// Atomic increment, returning the new value.
static inline uint64_t
atomic_dec_64_nv(uint64_t * ptr) {
    return __sync_sub_and_fetch(ptr, 1);
}

// Atomic increment, returning the new value.
static inline uint64_t
atomic_add_64_nv(uint64_t * ptr, int64_t amt) {
    return __sync_add_and_fetch(ptr, amt);
}

// Atomic increment, returning the new value.
static inline uint64_t
atomic_inc_64_nv(uint64_t * ptr) {
    return __sync_add_and_fetch(ptr, 1);
}

// The value stored in target is replaced with newval. The old value
// is returned by the function.
static inline uint64_t
atomic_swap_64(uint64_t * ptr, uint64_t newval) {
    return ck_pr_fas_64(ptr, newval);
}

// The membar_producer() function arranges for all  stores  issued
// before this  point  in  the  code to reach global visibility before
// any stores that follow.
static inline void
membar_producer(void) {
    ck_pr_fence_strict_store();
}

#ifdef  __cplusplus
}
#endif

#endif /* ATOMIC_H_03388ABD_5CE5_4B1C_815C_5258881CFB35 */
/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
