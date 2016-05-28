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

#ifndef FILE_H_F2A2E7C9_9795_434D_912C_04473DEA855D
#define FILE_H_F2A2E7C9_9795_434D_912C_04473DEA855D

#include <fcntl.h>
#include_next <sys/file.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* Glibc defined a number of the F flags, so make sure we have
 * compatible definitions.
 */
typedef enum {
    FREAD       = 0x01,
    FWRITE      = 0x02,
    _FNDELAY     = FNDELAY,
    _FAPPEND     = FAPPEND,
    FSYNC       = O_SYNC,
    FREVOKED    = 0x20,
    FDSYNC      = O_DSYNC,
    _FNONBLOCK   = FNONBLOCK,

    FCREAT      = O_CREAT,
    FTRUNC      = O_TRUNC,
    FOFFMAX     = O_LARGEFILE,
    FEXCL       = O_EXCL,
    FRSYNC      = O_RSYNC,
    FNODSYNC    = 0x10000,      /* fsync pseudo flag */
    FNOFOLLOW   = O_NOFOLLOW,   /* don't follow symlinks */
    FNOLINKS    = 0x40000,      /* don't allow multiple hard links */
    FIGNORECASE = 0x80000,      /* request case-insensitive lookups */

    FSEARCH     = 0x200000,
    FEXEC       = 0x400000,
    FCLOEXEC    = 0x800000,
} fflags_t;

typedef struct flock64 flock64_t;

#define F_FREESP 27 /* Free file space */

#ifdef  __cplusplus
}
#endif

#endif /* FILE_H_F2A2E7C9_9795_434D_912C_04473DEA855D */
