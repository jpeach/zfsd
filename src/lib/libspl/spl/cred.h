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

#ifndef CRED_H_53428638_1275_459E_9498_C1495F905F25
#define CRED_H_53428638_1275_459E_9498_C1495F905F25

#include <spl/zone.h>
#include <unistd.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct cred cred_t;

// A well-known handle to the current thread credential.
cred_t * CRED(void);

// Fully privileged credential (ie. superuser).
extern struct cred *kcred;

/* Return the effective UID. */
uid_t crgetuid(const cred_t *cr);

/* Return the real UID. */
uid_t crgetruid(const cred_t *cr);

/* Return the saved UID. */
uid_t crgetsuid(const cred_t *cr);

/* Return the effective GID. */
gid_t crgetgid(const cred_t *cr);

/* Return the real GID. */
gid_t crgetrgid(const cred_t *cr);

/* Return the saved GID. */
gid_t crgetsgid(const cred_t *cr);

/* Return the zone id from the user credential. */
zoneid_t crgetzoneid(const cred_t *cr);

const gid_t *crgetgroups(const cred_t *cr);

/* Returns the number of groups in the user credential. */
int crgetngroups(const cred_t *cr);


#ifdef  __cplusplus
}
#endif

#endif /* CRED_H_53428638_1275_459E_9498_C1495F905F25 */
