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

#ifndef TYPES_H_811D0750_7B93_4832_940A_8AF421DAF20A
#define TYPES_H_811D0750_7B93_4832_940A_8AF421DAF20A

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include_next <sys/types.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum { B_FALSE, B_TRUE } boolean_t;

typedef unsigned long ulong_t;
typedef unsigned long long u_longlong_t;
typedef long long longlong_t;
typedef unsigned int uint_t;
typedef unsigned char uchar_t;
typedef unsigned short ushort_t;

typedef uint_t minor_t;
typedef uint_t major_t;

typedef off_t lloff_t;
typedef off_t offset_t;
typedef u_longlong_t u_offset_t;

typedef uint64_t hrtime_t;

typedef short pri_t;

typedef struct page page_t;
typedef struct vnode vnode_t;

typedef int socket_t;

typedef int errno_t;

typedef int fd_t;

typedef short index_t;

typedef int zoneid_t;

// Errno value for success.
#if !defined(ESUCCESS)
#define ESUCCESS 0
#endif

typedef struct iovec iovec_t;
typedef u_longlong_t diskaddr_t;

#ifdef  __cplusplus
}
#endif

#endif /* TYPES_H_811D0750_7B93_4832_940A_8AF421DAF20A */
