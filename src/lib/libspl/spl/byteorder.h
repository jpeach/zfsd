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

#ifndef BYTEORDER_H_922E4DCE_ED41_4D36_8FB0_9E7FD4466AD9
#define BYTEORDER_H_922E4DCE_ED41_4D36_8FB0_9E7FD4466AD9

#include <endian.h>
#include <byteswap.h>
#include <string.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define LE_16(x)	le16toh(x)
#define LE_32(x)	le32toh(x)
#define LE_64(x)	le64toh(x)
#define BE_16(x)	be16toh(x)
#define BE_32(x)	be32toh(x)
#define BE_64(x)	be64toh(x)

/* Use __packed__ to get the compiler to generated an unaligned load. See 
 * https://www.kernel.org/doc/Documentation/unaligned-memory-access.txt
 */
#define UNALIGNED_READ(ptr, N) ({ \
	struct _u ## N { uint ## N ## _t value; } __attribute__ ((__packed__)); \
	((const struct _u ## N *)ptr)->value; \
})

#define LE_READ(ptr, N)  ({ LE_ ## N (UNALIGNED_READ(ptr, N)); })
#define BE_READ(ptr, N)  ({ BE_ ## N (UNALIGNED_READ(ptr, N)); })

// Read and convert unaligned bytes from the given address.
#define LE_IN16(ptr)	LE_READ(ptr, 16)
#define LE_IN32(ptr)	LE_READ(ptr, 32)
#define LE_IN64(ptr)	LE_READ(ptr, 64)
#define BE_IN16(ptr)	BE_READ(ptr, 16)
#define BE_IN32(ptr)	BE_READ(ptr, 32)
#define BE_IN64(ptr)	BE_READ(ptr, 64)

#define BSWAP_16(x) bswap_16(x)
#define BSWAP_32(x) bswap_32(x)
#define BSWAP_64(x) bswap_64(x)

#ifdef  __cplusplus
}
#endif

#endif /* BYTEORDER_H_922E4DCE_ED41_4D36_8FB0_9E7FD4466AD9 */
