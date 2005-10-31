/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_SPA_IMPL_H
#define	_SYS_SPA_IMPL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/spa.h>
#include <sys/vdev.h>
#include <sys/metaslab.h>
#include <sys/dmu.h>
#include <sys/dsl_pool.h>
#include <sys/uberblock_impl.h>
#include <sys/zfs_context.h>
#include <sys/avl.h>
#include <sys/refcount.h>
#include <sys/bplist.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct spa_config_lock {
	kmutex_t	scl_lock;
	uint64_t	scl_count;
	kthread_t	*scl_writer;
	kcondvar_t	scl_cv;
} spa_config_lock_t;

struct spa {
	/*
	 * Fields protected by spa_namespace_lock.
	 */
	char		*spa_name;
	avl_node_t	spa_avl;
	int		spa_anon;
	nvlist_t	*spa_config;
	uint64_t	spa_config_txg;		/* txg of last config change */
	spa_config_lock_t spa_config_lock;	/* configuration changes */
	kmutex_t	spa_config_cache_lock;	/* for spa_config RW_READER */
	int		spa_sync_pass;		/* iterate-to-convergence */
	int		spa_state;		/* pool state */
	uint8_t		spa_minref;		/* min refcnt of open pool */
	uint8_t		spa_traverse_wanted;	/* traverse lock wanted */
	taskq_t		*spa_vdev_retry_taskq;
	taskq_t		*spa_zio_issue_taskq[ZIO_TYPES];
	taskq_t		*spa_zio_intr_taskq[ZIO_TYPES];
	dsl_pool_t	*spa_dsl_pool;
	metaslab_class_t *spa_normal_class;	/* normal data class */
	uint64_t	spa_first_txg;		/* first txg after spa_open() */
	uint64_t	spa_freeze_txg;		/* freeze pool at this txg */
	objset_t	*spa_meta_objset;	/* copy of dp->dp_meta_objset */
	txg_list_t	spa_vdev_txg_list;	/* per-txg dirty vdev list */
	vdev_t		*spa_root_vdev;		/* top-level vdev container */
	list_t		spa_dirty_list;		/* vdevs with dirty labels */
	uint64_t	spa_config_object;	/* MOS object for pool config */
	uint64_t	spa_syncing_txg;	/* txg currently syncing */
	uint64_t	spa_sync_bplist_obj;	/* object for deferred frees */
	bplist_t	spa_sync_bplist;	/* deferred-free bplist */
	krwlock_t	spa_traverse_lock;	/* traverse vs. spa_sync() */
	uberblock_t	spa_ubsync;		/* last synced uberblock */
	uberblock_t	spa_uberblock;		/* current uberblock */
	kmutex_t	spa_scrub_lock;		/* resilver/scrub lock */
	kthread_t	*spa_scrub_thread;	/* scrub/resilver thread */
	traverse_handle_t *spa_scrub_th;	/* scrub traverse handle */
	uint64_t	spa_scrub_restart_txg;	/* need to restart */
	uint64_t	spa_scrub_maxtxg;	/* max txg we'll scrub */
	uint64_t	spa_scrub_inflight;	/* in-flight scrub I/Os */
	uint64_t	spa_scrub_errors;	/* scrub I/O error count */
	kcondvar_t	spa_scrub_cv;		/* scrub thread state change */
	kcondvar_t	spa_scrub_io_cv;	/* scrub I/O completion */
	uint8_t		spa_scrub_stop;		/* tell scrubber to stop */
	uint8_t		spa_scrub_suspend;	/* tell scrubber to suspend */
	uint8_t		spa_scrub_active;	/* active or suspended? */
	uint8_t		spa_scrub_type;		/* type of scrub we're doing */
	int		spa_sync_on;		/* sync threads are running */
	char		*spa_root;		/* alternate root directory */
	kmutex_t	spa_uberblock_lock;	/* vdev_uberblock_load_done() */
	/*
	 * spa_refcnt must be the last element because it changes size based on
	 * compilation options.  In order for the MDB module to function
	 * correctly, the other fields must remain in the same location.
	 */
	refcount_t	spa_refcount;		/* number of opens */
};

extern const char *spa_config_dir;
extern kmutex_t spa_namespace_lock;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPA_IMPL_H */
