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

#ifndef ZONE_H_BC65FF5B_081A_49D8_80B9_A6AF8351D70A
#define ZONE_H_BC65FF5B_081A_49D8_80B9_A6AF8351D70A

#include <spl/types.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct zone;
typedef struct zone zone_t;

#define GLOBAL_ZONEID   0
#define INGLOBALZONE(p) 1

extern zone_t *zone_find_by_any_path(const char *, boolean_t);
extern void zone_rele(zone_t *);

static inline unsigned long
zone_get_hostid(zone_t * z) {
    return gethostid();
}

#ifdef  __cplusplus
}
#endif

#endif /* ZONE_H_BC65FF5B_081A_49D8_80B9_A6AF8351D70A */
