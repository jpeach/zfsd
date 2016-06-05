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
#include <spl/kstat.h>
#include <spl/debug.h>

kstat_t *
kstat_create(const char * ks_module, int ks_instance, const char * ks_name,
        const char * ks_class, uchar_t ks_type, uint_t ks_ndata, uchar_t ks_flags)
{
    VERIFY3(ks_type, <, KSTAT_NUM_TYPES);

    return NULL;
}

void
kstat_install(kstat_t * ks)
{
    VERIFY3(ks, ==, NULL);
}

void
kstat_delete(kstat_t * ks)
{
    VERIFY3(ks, ==, NULL);
}

void
kstat_waitq_enter(kstat_io_t *kiop)
{
}

void
kstat_waitq_exit(kstat_io_t *kiop)
{
}

void
kstat_runq_enter(kstat_io_t *kiop)
{
}

void
kstat_runq_exit(kstat_io_t *kiop)
{
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
