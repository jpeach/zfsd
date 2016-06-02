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

#ifndef FM_H_67B98398_348E_4598_B03F_D5B2BB388547
#define FM_H_67B98398_348E_4598_B03F_D5B2BB388547

#include <spl/nvpair.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define FM_ENA_FMT1 0
#define FM_NVA_FREE 0
#define EVCH_SLEEP 0

#define FM_CLASS                        "class"
#define FM_VERSION                      "version"
#define FM_RSRC_RESOURCE                "resource"

#define FM_RSRC_VERSION                 0
#define FM_EREPORT_VERSION              0
#define FM_ZFS_SCHEME_VERSION           0

// For now we largely stub out the Fault Management routines. Later,
// if we need to, we can come back and pull in the Illumos code to
// create the proper error report.

nvlist_t * fm_nvlist_create(nv_alloc_t *nva);
void fm_nvlist_destroy(nvlist_t *report, int flag);
void fm_ereport_post(nvlist_t *report, int flag);
uint64_t fm_ena_generate(uint64_t timestamp, uchar_t format);
void fm_payload_set(nvlist_t *payload, ...);

void fm_ereport_set(nvlist_t *ereport, int version, const char *erpt_class,
        uint64_t ena, const nvlist_t *detector, ...);

void fm_fmri_zfs_set(nvlist_t *fmri, int version, uint64_t pool_guid,
        uint64_t vdev_guid);

#ifdef  __cplusplus
}
#endif

#endif /* FM_H_67B98398_348E_4598_B03F_D5B2BB388547 */
