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

#include <spl/types.h>
#include <spl/random.h>
#include <sys/syscall.h>
#include <linux/random.h>
#include <errno.h>

int random_get_pseudo_bytes(uint8_t *ptr, size_t len)
{
    while (len > 0) {
        // As of Fedora 23, there's no glibc wrapper for getrandom(2).
        int nread = syscall(SYS_getrandom, ptr, len, 0 /* flags */);
        if (nread == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                return -1;

            }
        }

        if (nread >= 0) {
            ptr += nread;
            len -= nread;
        }
    }

    return 0;
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
