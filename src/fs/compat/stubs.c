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

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/spa_boot.h>

// Area for ZFS kmem caches. Since we just use malloc(3), we don't have
// arenas at this time.
vmem_t *zio_alloc_arena = NULL;

extern int zfs_deadman_enabled;

void spa_arch_init()
{
    // Disable zfs deadman by defaul, like Illumos on sparc.
    zfs_deadman_enabled = 0;
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
