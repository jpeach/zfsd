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
#include <spl/cred.h>

struct cred{};

static struct cred cred0;
static struct cred current;

struct cred *kcred = &cred0;

cred_t * CRED(void)
{
    return &current;
}

static void
cred_getresuid(const cred_t *cr, uid_t *ruid, uid_t *euid, uid_t *suid)
{
    if (cr == CRED()) {
        getresuid(ruid, euid, suid);
        return;
    }

    if (cr == kcred) {
        *ruid = *euid = *suid = 0;
        return;
    }

    *ruid = *euid = *suid = -1;
}

static void
cred_getresgid(const cred_t *cr, gid_t *rgid, gid_t *egid, gid_t *sgid)
{
    if (cr == CRED()) {
        getresgid(rgid, egid, sgid);
        return;
    }

    if (cr == kcred) {
        *rgid = *egid = *sgid = 0;
        return;
    }

    *rgid = *egid = *sgid = -1;
}

/* Return the real UID. */
uid_t crgetruid(const cred_t *cr)
{
    uid_t ruid, euid, suid;
    cred_getresuid(cr, &ruid, &euid, &suid);

    return ruid;
}

/* Return the effective UID. */
uid_t crgetuid(const cred_t *cr)
{
    uid_t ruid, euid, suid;
    cred_getresuid(cr, &ruid, &euid, &suid);

    return euid;
}

/* Return the saved UID. */
uid_t crgetsuid(const cred_t *cr)
{
    uid_t ruid, euid, suid;
    cred_getresuid(cr, &ruid, &euid, &suid);

    return suid;
}

/* Return the effective GID. */
gid_t crgetgid(const cred_t *cr)
{
    gid_t rgid, egid, sgid;
    cred_getresgid(cr, &rgid, &egid, &sgid);

    return egid;
}

/* Return the real GID. */
gid_t crgetrgid(const cred_t *cr)
{
    gid_t rgid, egid, sgid;
    cred_getresgid(cr, &rgid, &egid, &sgid);

    return egid;
}

/* Return the saved GID. */
gid_t crgetsgid(const cred_t *cr)
{
    gid_t rgid, egid, sgid;
    cred_getresgid(cr, &rgid, &egid, &sgid);

    return egid;
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
