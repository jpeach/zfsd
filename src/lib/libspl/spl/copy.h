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

#ifndef COPY_H_3CF40C66_24D5_4E69_BFA5_C7BB21C6233E
#define COPY_H_3CF40C66_24D5_4E69_BFA5_C7BB21C6233E

#include <string.h>

#ifdef  __cplusplus
extern "C" {
#endif

int copyinstr(const char *src, char *dst, size_t max_len, size_t *copied);
int copystr(const char *src, char *dst, size_t max_len, size_t *outlen);
void ovbcopy(const void *src, void *dst, size_t len);

static inline int
copyin(const void *src, void *dst, size_t nbytes)
{
	memcpy(dst, src, nbytes);
	return 0;
}

static inline int
copyout(const void *src, void *dst, size_t nbytes)
{
	memcpy(dst, src, nbytes);
	return 0;
}

#ifdef  __cplusplus
}
#endif

#endif /* COPY_H_3CF40C66_24D5_4E69_BFA5_C7BB21C6233E */
