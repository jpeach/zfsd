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

#include <signal.h>

#include <phenom/defs.h>
#include <phenom/log.h>
#include <phenom/job.h>
#include <phenom/sysutil.h>

#include <spl/mutex.h>
#include <spl/rwlock.h>
#include <spl/condvar.h>
#include <sys/zfs_vfsops.h>

#include "init.h"

extern uint_t rrw_tsd_key;

// From zfs_ioctl.c.
uint_t zfs_fsyncer_key;

void
zfsd_init_phenom(void)
{
    signal(SIGPIPE, SIG_IGN);

    VERIFY3(ph_library_init(), ==, PH_OK);
    VERIFY3(ph_nbio_init(0), ==, PH_OK);
}

void
zfsd_init_zfs(void)
{
    // See zfs_ioctl.c:_init() for ZFS initialization order.
    system_taskq_init();
    spa_init(FREAD | FWRITE);

	tsd_create(&rrw_tsd_key, rrw_tsd_destroy);
	tsd_create(&zfs_fsyncer_key, NULL);

#if 0
    // zfs_allow_log_key is internal to zfs_ioctl.c. Used to obtain the SPA log
    // history, see zfs_ioc_log_history().

	tsd_create(&zfs_allow_log_key, zfs_allow_log_destroy);
#endif
}

void
zfsd_run(void)
{
    ph_sched_run();
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
