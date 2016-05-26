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
#include <spl/mutex.h>
#include <spl/rwlock.h>
#include <spl/condvar.h>
#include <spl/policy.h>
#include <errno.h>

#define SECPOLICY_DEFAULT(cr) do { \
    if (cr == kcred) { return 0; } \
    return EPERM; \
} while (0) \

// TODO: The following secpolicy stubs are just for linking. Clearly we ought
// to implement these in some way ...

int secpolicy_vnode_access2(const cred_t *cr, vnode_t *vp,
        uid_t owner, mode_t current, mode_t wanted)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_remove(const cred_t *cr)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_zfs(const cred_t *cr)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_setdac(const cred_t *cr, uid_t owner)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_setattr(cred_t *cr, struct vnode *vp, struct vattr *vap,
        const struct vattr *ovap, int flags,
        int unlocked_access(void *, int, cred_t *),
        void *node)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_chown(const cred_t *cr, uid_t owner)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_setids_setgids(const cred_t *cr, gid_t gid)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_setid_retain(const cred_t *cr, boolean_t issuidroot)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_create_gid(const cred_t *cr)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_vnode_stky_modify(const cred_t *cr)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_basic_link(const cred_t *cr)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_fs_mount(cred_t *cr, vnode_t *mvp, struct vfs *vfsp)
{
    SECPOLICY_DEFAULT(cr);
}

void secpolicy_fs_mount_clearopts(cred_t *cr, struct vfs *vfsp)
{
}

void secpolicy_setid_clear(vattr_t *vap, cred_t *cr)
{
}

int secpolicy_setid_setsticky_clear(vnode_t *vp, vattr_t *vap,
        const vattr_t *ovap, cred_t *cr)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_sys_config(const cred_t *cr, boolean_t checkonly)
{
    SECPOLICY_DEFAULT(cr);
}

int secpolicy_xvattr(xvattr_t *xvap, uid_t owner, cred_t *cr, vtype_t vtype)
{
    SECPOLICY_DEFAULT(cr);
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
