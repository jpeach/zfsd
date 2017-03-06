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

#ifndef SDT_H_896B5066_A65A_4BFE_9E50_8098BAE6D5BE
#define SDT_H_896B5066_A65A_4BFE_9E50_8098BAE6D5BE

#include <errno.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define DTRACE_PROBE(...)
#define DTRACE_PROBE1(...)
#define DTRACE_PROBE2(...)
#define DTRACE_PROBE3(...)
#define DTRACE_PROBE4(...)
#define DTRACE_PROBE5(...)

#if DEBUG
extern int SET_ERROR(int);
#else
#define SET_ERROR(error) ({errno = (error);})
#endif


#ifdef  __cplusplus
}
#endif

#endif /* SDT_H_896B5066_A65A_4BFE_9E50_8098BAE6D5BE */
