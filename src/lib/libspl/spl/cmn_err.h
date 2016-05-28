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

#ifndef CMN_ERR_H_C6701B92_2F67_4F62_B839_4820E8A99215
#define CMN_ERR_H_C6701B92_2F67_4F62_B839_4820E8A99215

#include <stdarg.h>
#include <stdio.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    CE_CONT         = 0,      /* continuation         */
    CE_NOTE         = 1,      /* notice               */
    CE_WARN         = 2,      /* warning              */
    CE_PANIC        = 3,      /* panic                */
    CE_IGNORE       = 4,      /* print nothing        */
} cmn_err_level_t;

void vcmn_err(int ce, const char * fmt, va_list adx);

void cmn_err(int ce, const char * fmt, ...)
    __attribute__((format(printf, 2, 3)));

void vpanic(const char * fmt, va_list adx)
    __attribute__((noreturn));

void panic(const char * fmt, ...)
    __attribute__((format(printf, 1, 2)))
    __attribute__((noreturn));

#define fm_panic panic

#ifdef  __cplusplus
}
#endif

#endif /* CMN_ERR_H_C6701B92_2F67_4F62_B839_4820E8A99215 */
