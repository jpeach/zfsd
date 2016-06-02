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
#include <spl/fm.h>
#include <ck_pr.h>

static uint32_t ena_counter;

uint64_t
fm_ena_generate(uint64_t timestamp, uchar_t format)
{
    if (timestamp) {
        timestamp = gethrestime_sec();
    }

    // Not really and ENA, just a unique-ish number. For a description of
    // ENAs, see uts/common/os/fm.c and the fm_ena_time_get(), etc APIs.
    return timestamp << 32 | ck_pr_faa_32(&ena_counter, 1);
}

nvlist_t * fm_nvlist_create(nv_alloc_t *nva)
{
    return fnvlist_alloc();
}

void fm_nvlist_destroy(nvlist_t *report, int flag)
{
    fnvlist_free(report);
}

void fm_ereport_post(nvlist_t *report, int flag)
{
    // TODO
}

void fm_payload_set(nvlist_t *payload, ...)
{
    // TODO
}

void fm_ereport_set(nvlist_t *ereport, int version, const char *erpt_class,
        uint64_t ena, const nvlist_t *detector, ...)
{
    // TODO
}

void fm_fmri_zfs_set(nvlist_t *fmri, int version, uint64_t pool_guid,
        uint64_t vdev_guid)
{
    // TODO
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
