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

#ifndef DEBUG_H_AACBD5C7_4631_4365_9AC9_4A316F0E57DB
#define DEBUG_H_AACBD5C7_4631_4365_9AC9_4A316F0E57DB

#include <sys/cdefs.h>
#include <assert.h>

#ifdef  __cplusplus
extern "C" {
#endif

/* From <assert.h>. Defining NDEBUG hides this declaration but we still
 * need it for the VERIFY macros.
 */
extern void __assert_fail (const char *__assertion, const char *__file,
                           unsigned int __line, const char *__function)
     __THROW __attribute__ ((__noreturn__));

/* See Illumos usr/src/uts/common/sys/debug.h. */

#define ASSERT(expr)            assert( expr )
#define ASSERT3P(lhs, op, rhs)  assert( (lhs) op (rhs) )
#define ASSERT3U(lhs, op, rhs)  assert( (lhs) op (rhs) )
#define ASSERT3S(lhs, op, rhs)  assert( (lhs) op (rhs) )
#define ASSERT0(expr)           assert( (expr) == 0 )

#define CTASSERT(expr) static_assert(expr, "")

#define VERIFY(expr) do { \
    typeof(expr) _r = (expr); \
    if (!(_r))  { \
        __assert_fail(__STRING(expr), __FILE__, __LINE__, __func__); \
    } \
} while (0)

#define VERIFY3(lhs, op, rhs) do { \
    typeof(lhs) _l = (lhs); \
    typeof(rhs) _r = (rhs); \
    if (!(_l op _r))  { \
        __assert_fail(__STRING(lhs) " " __STRING(op) " " __STRING(rhs), __FILE__, __LINE__, __func__); \
    } \
} while (0)

#define VERIFY0(expr)               VERIFY3(expr, ==, 0)
#define VERIFY3S(lhs, expr, rhs)    VERIFY3(lhs, expr, rhs)
#define VERIFY3U(lhs, expr, rhs)    VERIFY3(lhs, expr, rhs)
#define VERIFY3P(lhs, expr, rhs)    VERIFY3(lhs, expr, rhs)

#define IMPLY(pre, post) do { \
    if ((pre) && !(post)) { \
        __assert_fail(__STRING(pre) " implies " __STRING(post), __FILE__, __LINE__, __func__); \
    } \
} while (0)

#define EQUIV(A, B) do { \
    if (!!(A) != !!(B)) { \
        __assert_fail("(" __STRING(A) ") is equivalent to (" __STRING(B) ")", __FILE__, __LINE__, __func__); \
    } \
} while (0)

#ifdef  __cplusplus
}
#endif

#endif /* DEBUG_H_AACBD5C7_4631_4365_9AC9_4A316F0E57DB */
