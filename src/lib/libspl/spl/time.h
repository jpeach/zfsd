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

#ifndef TIME_H_8A6940E8_1A9B_4522_8FC9_BC0502388D88
#define TIME_H_8A6940E8_1A9B_4522_8FC9_BC0502388D88

#include_next <sys/time.h>

#ifdef  __cplusplus
extern "C" {
#endif

// NOTE: clock_t comes from glibc and is defined to be __SYSCALL_SLONG_TYPE, so
// at least on Linux x86_64 it ought to be a signed long.

#define SEC             1ul
#define MILLISEC        1000ul
#define MICROSEC        1000000ul
#define NANOSEC         1000000000ul

#define NSEC_PER_SEC    (NANOSEC / SEC)
#define USEC_PER_SEC    (MICROSEC / SEC)
#define NSEC_PER_USEC   (NANOSEC / MICROSEC)
#define NSEC_PER_MSEC   (NANOSEC / MILLISEC)

#define SEC_TO_USEC(sec)    ((sec) * USEC_PER_SEC)
#define USEC_TO_SEC(usec)   ((usec) / USEC_PER_SEC)

#define USEC_TO_NSEC(usec)  ((usec) * NSEC_PER_USEC)
#define NSEC_TO_USEC(nsec)  ((nsec) / NSEC_PER_USEC)

#define SEC_TO_NSEC(sec)    ((sec) * NSEC_PER_SEC)
#define NSEC_TO_SEC(nsec)   ((nsec) / NSEC_PER_SEC)

// ddi_get_lbolt() returns usec, so (logical) clock ticks are in
// usec. That means hz is USEC.
#define hz USEC_PER_SEC

/* Get current uptime (logical ticks) in usec. */
static inline clock_t ddi_get_lbolt(void) {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return SEC_TO_USEC(ts.tv_sec) + NSEC_TO_USEC(ts.tv_nsec);
}

#ifdef  __cplusplus
}
#endif

#endif /* TIME_H_8A6940E8_1A9B_4522_8FC9_BC0502388D88 */
