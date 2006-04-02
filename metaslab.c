/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/zfs_context.h>
#include <sys/spa_impl.h>
#include <sys/dmu.h>
#include <sys/dmu_tx.h>
#include <sys/space_map.h>
#include <sys/metaslab_impl.h>
#include <sys/vdev_impl.h>
#include <sys/zio.h>

/*
 * ==========================================================================
 * Metaslab classes
 * ==========================================================================
 */
metaslab_class_t *
metaslab_class_create(void)
{
	metaslab_class_t *mc;

	mc = kmem_zalloc(sizeof (metaslab_class_t), KM_SLEEP);

	mc->mc_rotor = NULL;

	return (mc);
}

void
metaslab_class_destroy(metaslab_class_t *mc)
{
	metaslab_group_t *mg;

	while ((mg = mc->mc_rotor) != NULL) {
		metaslab_class_remove(mc, mg);
		metaslab_group_destroy(mg);
	}

	kmem_free(mc, sizeof (metaslab_class_t));
}

void
metaslab_class_add(metaslab_class_t *mc, metaslab_group_t *mg)
{
	metaslab_group_t *mgprev, *mgnext;

	ASSERT(mg->mg_class == NULL);

	if ((mgprev = mc->mc_rotor) == NULL) {
		mg->mg_prev = mg;
		mg->mg_next = mg;
	} else {
		mgnext = mgprev->mg_next;
		mg->mg_prev = mgprev;
		mg->mg_next = mgnext;
		mgprev->mg_next = mg;
		mgnext->mg_prev = mg;
	}
	mc->mc_rotor = mg;
	mg->mg_class = mc;
}

void
metaslab_class_remove(metaslab_class_t *mc, metaslab_group_t *mg)
{
	metaslab_group_t *mgprev, *mgnext;

	ASSERT(mg->mg_class == mc);

	mgprev = mg->mg_prev;
	mgnext = mg->mg_next;

	if (mg == mgnext) {
		mc->mc_rotor = NULL;
	} else {
		mc->mc_rotor = mgnext;
		mgprev->mg_next = mgnext;
		mgnext->mg_prev = mgprev;
	}

	mg->mg_prev = NULL;
	mg->mg_next = NULL;
	mg->mg_class = NULL;
}

/*
 * ==========================================================================
 * Metaslab groups
 * ==========================================================================
 */
static int
metaslab_compare(const void *x1, const void *x2)
{
	const metaslab_t *m1 = x1;
	const metaslab_t *m2 = x2;

	if (m1->ms_weight < m2->ms_weight)
		return (1);
	if (m1->ms_weight > m2->ms_weight)
		return (-1);

	/*
	 * If the weights are identical, use the offset to force uniqueness.
	 */
	if (m1->ms_map.sm_start < m2->ms_map.sm_start)
		return (-1);
	if (m1->ms_map.sm_start > m2->ms_map.sm_start)
		return (1);

	ASSERT3P(m1, ==, m2);

	return (0);
}

metaslab_group_t *
metaslab_group_create(metaslab_class_t *mc, vdev_t *vd)
{
	metaslab_group_t *mg;

	mg = kmem_zalloc(sizeof (metaslab_group_t), KM_SLEEP);
	mutex_init(&mg->mg_lock, NULL, MUTEX_DEFAULT, NULL);
	avl_create(&mg->mg_metaslab_tree, metaslab_compare,
	    sizeof (metaslab_t), offsetof(struct metaslab, ms_group_node));
	mg->mg_aliquot = 2ULL << 20;		/* XXX -- tweak me */
	mg->mg_vd = vd;
	metaslab_class_add(mc, mg);

	return (mg);
}

void
metaslab_group_destroy(metaslab_group_t *mg)
{
	avl_destroy(&mg->mg_metaslab_tree);
	mutex_destroy(&mg->mg_lock);
	kmem_free(mg, sizeof (metaslab_group_t));
}

static void
metaslab_group_add(metaslab_group_t *mg, metaslab_t *msp)
{
	mutex_enter(&mg->mg_lock);
	ASSERT(msp->ms_group == NULL);
	msp->ms_group = mg;
	msp->ms_weight = 0;
	avl_add(&mg->mg_metaslab_tree, msp);
	mutex_exit(&mg->mg_lock);
}

static void
metaslab_group_remove(metaslab_group_t *mg, metaslab_t *msp)
{
	mutex_enter(&mg->mg_lock);
	ASSERT(msp->ms_group == mg);
	avl_remove(&mg->mg_metaslab_tree, msp);
	msp->ms_group = NULL;
	mutex_exit(&mg->mg_lock);
}

static void
metaslab_group_sort(metaslab_group_t *mg, metaslab_t *msp, uint64_t weight)
{
	ASSERT(MUTEX_HELD(&msp->ms_lock));

	mutex_enter(&mg->mg_lock);
	ASSERT(msp->ms_group == mg);
	avl_remove(&mg->mg_metaslab_tree, msp);
	msp->ms_weight = weight;
	avl_add(&mg->mg_metaslab_tree, msp);
	mutex_exit(&mg->mg_lock);
}

/*
 * ==========================================================================
 * The first-fit block allocator
 * ==========================================================================
 */
static void
metaslab_ff_load(space_map_t *sm)
{
	ASSERT(sm->sm_ppd == NULL);
	sm->sm_ppd = kmem_zalloc(64 * sizeof (uint64_t), KM_SLEEP);
}

static void
metaslab_ff_unload(space_map_t *sm)
{
	kmem_free(sm->sm_ppd, 64 * sizeof (uint64_t));
	sm->sm_ppd = NULL;
}

static uint64_t
metaslab_ff_alloc(space_map_t *sm, uint64_t size)
{
	avl_tree_t *t = &sm->sm_root;
	uint64_t align = size & -size;
	uint64_t *cursor = (uint64_t *)sm->sm_ppd + highbit(align) - 1;
	space_seg_t *ss, ssearch;
	avl_index_t where;

	ssearch.ss_start = *cursor;
	ssearch.ss_end = *cursor + size;

	ss = avl_find(t, &ssearch, &where);
	if (ss == NULL)
		ss = avl_nearest(t, where, AVL_AFTER);

	while (ss != NULL) {
		uint64_t offset = P2ROUNDUP(ss->ss_start, align);

		if (offset + size <= ss->ss_end) {
			*cursor = offset + size;
			return (offset);
		}
		ss = AVL_NEXT(t, ss);
	}

	/*
	 * If we know we've searched the whole map (*cursor == 0), give up.
	 * Otherwise, reset the cursor to the beginning and try again.
	 */
	if (*cursor == 0)
		return (-1ULL);

	*cursor = 0;
	return (metaslab_ff_alloc(sm, size));
}

/* ARGSUSED */
static void
metaslab_ff_claim(space_map_t *sm, uint64_t start, uint64_t size)
{
	/* No need to update cursor */
}

/* ARGSUSED */
static void
metaslab_ff_free(space_map_t *sm, uint64_t start, uint64_t size)
{
	/* No need to update cursor */
}

static space_map_ops_t metaslab_ff_ops = {
	metaslab_ff_load,
	metaslab_ff_unload,
	metaslab_ff_alloc,
	metaslab_ff_claim,
	metaslab_ff_free
};

/*
 * ==========================================================================
 * Metaslabs
 * ==========================================================================
 */
metaslab_t *
metaslab_init(metaslab_group_t *mg, space_map_obj_t *smo,
	uint64_t start, uint64_t size, uint64_t txg)
{
	vdev_t *vd = mg->mg_vd;
	metaslab_t *msp;

	msp = kmem_zalloc(sizeof (metaslab_t), KM_SLEEP);

	msp->ms_smo_syncing = *smo;

	/*
	 * We create the main space map here, but we don't create the
	 * allocmaps and freemaps until metaslab_sync_done().  This serves
	 * two purposes: it allows metaslab_sync_done() to detect the
	 * addition of new space; and for debugging, it ensures that we'd
	 * data fault on any attempt to use this metaslab before it's ready.
	 */
	space_map_create(&msp->ms_map, start, size,
	    vd->vdev_ashift, &msp->ms_lock);

	metaslab_group_add(mg, msp);

	/*
	 * If we're opening an existing pool (txg == 0) or creating
	 * a new one (txg == TXG_INITIAL), all space is available now.
	 * If we're adding space to an existing pool, the new space
	 * does not become available until after this txg has synced.
	 */
	if (txg <= TXG_INITIAL)
		metaslab_sync_done(msp, 0);

	if (txg != 0) {
		/*
		 * The vdev is dirty, but the metaslab isn't -- it just needs
		 * to have metaslab_sync_done() invoked from vdev_sync_done().
		 * [We could just dirty the metaslab, but that would cause us
		 * to allocate a space map object for it, which is wasteful
		 * and would mess up the locality logic in metaslab_weight().]
		 */
		ASSERT(TXG_CLEAN(txg) == spa_last_synced_txg(vd->vdev_spa));
		vdev_dirty(vd, 0, NULL, txg);
		vdev_dirty(vd, VDD_METASLAB, msp, TXG_CLEAN(txg));
	}

	return (msp);
}

void
metaslab_fini(metaslab_t *msp)
{
	metaslab_group_t *mg = msp->ms_group;
	int t;

	vdev_space_update(mg->mg_vd, -msp->ms_map.sm_size,
	    -msp->ms_smo.smo_alloc);

	metaslab_group_remove(mg, msp);

	mutex_enter(&msp->ms_lock);

	space_map_unload(&msp->ms_map);
	space_map_destroy(&msp->ms_map);

	for (t = 0; t < TXG_SIZE; t++) {
		space_map_destroy(&msp->ms_allocmap[t]);
		space_map_destroy(&msp->ms_freemap[t]);
	}

	mutex_exit(&msp->ms_lock);

	kmem_free(msp, sizeof (metaslab_t));
}

#define	METASLAB_ACTIVE_WEIGHT	(1ULL << 63)

static uint64_t
metaslab_weight(metaslab_t *msp)
{
	space_map_t *sm = &msp->ms_map;
	space_map_obj_t *smo = &msp->ms_smo;
	vdev_t *vd = msp->ms_group->mg_vd;
	uint64_t weight, space;

	ASSERT(MUTEX_HELD(&msp->ms_lock));

	/*
	 * The baseline weight is the metaslab's free space.
	 */
	space = sm->sm_size - smo->smo_alloc;
	weight = space;

	/*
	 * Modern disks have uniform bit density and constant angular velocity.
	 * Therefore, the outer recording zones are faster (higher bandwidth)
	 * than the inner zones by the ratio of outer to inner track diameter,
	 * which is typically around 2:1.  We account for this by assigning
	 * higher weight to lower metaslabs (multiplier ranging from 2x to 1x).
	 * In effect, this means that we'll select the metaslab with the most
	 * free bandwidth rather than simply the one with the most free space.
	 */
	weight = 2 * weight -
	    ((sm->sm_start >> vd->vdev_ms_shift) * weight) / vd->vdev_ms_count;
	ASSERT(weight >= space && weight <= 2 * space);

	/*
	 * For locality, assign higher weight to metaslabs we've used before.
	 */
	if (smo->smo_object != 0)
		weight *= 2;
	ASSERT(weight >= space && weight <= 4 * space);

	/*
	 * If this metaslab is one we're actively using, adjust its weight to
	 * make it preferable to any inactive metaslab so we'll polish it off.
	 */
	weight |= (msp->ms_weight & METASLAB_ACTIVE_WEIGHT);

	return (weight);
}

static int
metaslab_activate(metaslab_t *msp)
{
	space_map_t *sm = &msp->ms_map;

	ASSERT(MUTEX_HELD(&msp->ms_lock));

	if (msp->ms_weight < METASLAB_ACTIVE_WEIGHT) {
		int error = space_map_load(sm, &metaslab_ff_ops,
		    SM_FREE, &msp->ms_smo,
		    msp->ms_group->mg_vd->vdev_spa->spa_meta_objset);
		if (error) {
			metaslab_group_sort(msp->ms_group, msp, 0);
			return (error);
		}
		metaslab_group_sort(msp->ms_group, msp,
		    msp->ms_weight | METASLAB_ACTIVE_WEIGHT);
	}
	ASSERT(sm->sm_loaded);
	ASSERT(msp->ms_weight >= METASLAB_ACTIVE_WEIGHT);

	return (0);
}

static void
metaslab_passivate(metaslab_t *msp, uint64_t size)
{
	metaslab_group_sort(msp->ms_group, msp, MIN(msp->ms_weight, size - 1));
	ASSERT(msp->ms_weight < METASLAB_ACTIVE_WEIGHT);
}

/*
 * Write a metaslab to disk in the context of the specified transaction group.
 */
void
metaslab_sync(metaslab_t *msp, uint64_t txg)
{
	vdev_t *vd = msp->ms_group->mg_vd;
	spa_t *spa = vd->vdev_spa;
	objset_t *mos = spa->spa_meta_objset;
	space_map_t *allocmap = &msp->ms_allocmap[txg & TXG_MASK];
	space_map_t *freemap = &msp->ms_freemap[txg & TXG_MASK];
	space_map_t *freed_map = &msp->ms_freemap[TXG_CLEAN(txg) & TXG_MASK];
	space_map_t *sm = &msp->ms_map;
	space_map_obj_t *smo = &msp->ms_smo_syncing;
	dmu_buf_t *db;
	dmu_tx_t *tx;
	int t;

	tx = dmu_tx_create_assigned(spa_get_dsl(spa), txg);

	/*
	 * The only state that can actually be changing concurrently with
	 * metaslab_sync() is the metaslab's ms_map.  No other thread can
	 * be modifying this txg's allocmap, freemap, freed_map, or smo.
	 * Therefore, we only hold ms_lock to satify space_map ASSERTs.
	 * We drop it whenever we call into the DMU, because the DMU
	 * can call down to us (e.g. via zio_free()) at any time.
	 */
	mutex_enter(&msp->ms_lock);

	if (smo->smo_object == 0) {
		ASSERT(smo->smo_objsize == 0);
		ASSERT(smo->smo_alloc == 0);
		mutex_exit(&msp->ms_lock);
		smo->smo_object = dmu_object_alloc(mos,
		    DMU_OT_SPACE_MAP, 1 << SPACE_MAP_BLOCKSHIFT,
		    DMU_OT_SPACE_MAP_HEADER, sizeof (*smo), tx);
		ASSERT(smo->smo_object != 0);
		dmu_write(mos, vd->vdev_ms_array, sizeof (uint64_t) *
		    (sm->sm_start >> vd->vdev_ms_shift),
		    sizeof (uint64_t), &smo->smo_object, tx);
		mutex_enter(&msp->ms_lock);
	}

	space_map_walk(freemap, space_map_add, freed_map);

	if (sm->sm_loaded && spa_sync_pass(spa) == 1 && smo->smo_objsize >=
	    2 * sizeof (uint64_t) * avl_numnodes(&sm->sm_root)) {
		/*
		 * The in-core space map representation is twice as compact
		 * as the on-disk one, so it's time to condense the latter
		 * by generating a pure allocmap from first principles.
		 *
		 * This metaslab is 100% allocated,
		 * minus the content of the in-core map (sm),
		 * minus what's been freed this txg (freed_map),
		 * minus allocations from txgs in the future
		 * (because they haven't been committed yet).
		 */
		space_map_vacate(allocmap, NULL, NULL);
		space_map_vacate(freemap, NULL, NULL);

		space_map_add(allocmap, allocmap->sm_start, allocmap->sm_size);

		space_map_walk(sm, space_map_remove, allocmap);
		space_map_walk(freed_map, space_map_remove, allocmap);

		for (t = 1; t < TXG_CONCURRENT_STATES; t++)
			space_map_walk(&msp->ms_allocmap[(txg + t) & TXG_MASK],
			    space_map_remove, allocmap);

		mutex_exit(&msp->ms_lock);
		space_map_truncate(smo, mos, tx);
		mutex_enter(&msp->ms_lock);
	}

	space_map_sync(allocmap, SM_ALLOC, smo, mos, tx);
	space_map_sync(freemap, SM_FREE, smo, mos, tx);

	mutex_exit(&msp->ms_lock);

	VERIFY(0 == dmu_bonus_hold(mos, smo->smo_object, FTAG, &db));
	dmu_buf_will_dirty(db, tx);
	ASSERT3U(db->db_size, ==, sizeof (*smo));
	bcopy(smo, db->db_data, db->db_size);
	dmu_buf_rele(db, FTAG);

	dmu_tx_commit(tx);
}

/*
 * Called after a transaction group has completely synced to mark
 * all of the metaslab's free space as usable.
 */
void
metaslab_sync_done(metaslab_t *msp, uint64_t txg)
{
	space_map_obj_t *smo = &msp->ms_smo;
	space_map_obj_t *smosync = &msp->ms_smo_syncing;
	space_map_t *sm = &msp->ms_map;
	space_map_t *freed_map = &msp->ms_freemap[TXG_CLEAN(txg) & TXG_MASK];
	metaslab_group_t *mg = msp->ms_group;
	vdev_t *vd = mg->mg_vd;
	int t;

	mutex_enter(&msp->ms_lock);

	/*
	 * If this metaslab is just becoming available, initialize its
	 * allocmaps and freemaps and add its capacity to the vdev.
	 */
	if (freed_map->sm_size == 0) {
		for (t = 0; t < TXG_SIZE; t++) {
			space_map_create(&msp->ms_allocmap[t], sm->sm_start,
			    sm->sm_size, sm->sm_shift, sm->sm_lock);
			space_map_create(&msp->ms_freemap[t], sm->sm_start,
			    sm->sm_size, sm->sm_shift, sm->sm_lock);
		}
		vdev_space_update(vd, sm->sm_size, 0);
	}

	vdev_space_update(vd, 0, smosync->smo_alloc - smo->smo_alloc);

	ASSERT(msp->ms_allocmap[txg & TXG_MASK].sm_space == 0);
	ASSERT(msp->ms_freemap[txg & TXG_MASK].sm_space == 0);

	/*
	 * If there's a space_map_load() in progress, wait for it to complete
	 * so that we have a consistent view of the in-core space map.
	 * Then, add everything we freed in this txg to the map.
	 */
	space_map_load_wait(sm);
	space_map_vacate(freed_map, sm->sm_loaded ? space_map_free : NULL, sm);

	*smo = *smosync;

	/*
	 * If the map is loaded but no longer active, evict it as soon as all
	 * future allocations have synced.  (If we unloaded it now and then
	 * loaded a moment later, the map wouldn't reflect those allocations.)
	 */
	if (sm->sm_loaded && msp->ms_weight < METASLAB_ACTIVE_WEIGHT) {
		int evictable = 1;

		for (t = 1; t < TXG_CONCURRENT_STATES; t++)
			if (msp->ms_allocmap[(txg + t) & TXG_MASK].sm_space)
				evictable = 0;

		if (evictable)
			space_map_unload(sm);
	}

	metaslab_group_sort(mg, msp, metaslab_weight(msp));

	mutex_exit(&msp->ms_lock);
}

/*
 * Intent log support: upon opening the pool after a crash, notify the SPA
 * of blocks that the intent log has allocated for immediate write, but
 * which are still considered free by the SPA because the last transaction
 * group didn't commit yet.
 */
int
metaslab_claim(spa_t *spa, dva_t *dva, uint64_t txg)
{
	uint64_t vdev = DVA_GET_VDEV(dva);
	uint64_t offset = DVA_GET_OFFSET(dva);
	uint64_t size = DVA_GET_ASIZE(dva);
	vdev_t *vd;
	metaslab_t *msp;
	int error;

	if ((vd = vdev_lookup_top(spa, vdev)) == NULL)
		return (ENXIO);

	if ((offset >> vd->vdev_ms_shift) >= vd->vdev_ms_count)
		return (ENXIO);

	msp = vd->vdev_ms[offset >> vd->vdev_ms_shift];

	if (DVA_GET_GANG(dva))
		size = vdev_psize_to_asize(vd, SPA_GANGBLOCKSIZE);

	mutex_enter(&msp->ms_lock);

	error = metaslab_activate(msp);
	if (error) {
		mutex_exit(&msp->ms_lock);
		return (error);
	}

	if (msp->ms_allocmap[txg & TXG_MASK].sm_space == 0)
		vdev_dirty(vd, VDD_METASLAB, msp, txg);

	space_map_claim(&msp->ms_map, offset, size);
	space_map_add(&msp->ms_allocmap[txg & TXG_MASK], offset, size);

	mutex_exit(&msp->ms_lock);

	return (0);
}

static metaslab_t *
metaslab_group_alloc(metaslab_group_t *mg, uint64_t size, uint64_t *offp,
	uint64_t txg)
{
	metaslab_t *msp = NULL;
	uint64_t offset = -1ULL;

	for (;;) {
		mutex_enter(&mg->mg_lock);
		msp = avl_first(&mg->mg_metaslab_tree);
		if (msp == NULL || msp->ms_weight < size) {
			mutex_exit(&mg->mg_lock);
			return (NULL);
		}
		mutex_exit(&mg->mg_lock);

		mutex_enter(&msp->ms_lock);

		if (metaslab_activate(msp) != 0) {
			mutex_exit(&msp->ms_lock);
			continue;
		}

		if ((offset = space_map_alloc(&msp->ms_map, size)) != -1ULL)
			break;

		metaslab_passivate(msp, size);

		mutex_exit(&msp->ms_lock);
	}

	if (msp->ms_allocmap[txg & TXG_MASK].sm_space == 0)
		vdev_dirty(mg->mg_vd, VDD_METASLAB, msp, txg);

	space_map_add(&msp->ms_allocmap[txg & TXG_MASK], offset, size);

	mutex_exit(&msp->ms_lock);

	*offp = offset;
	return (msp);
}

/*
 * Allocate a block for the specified i/o.
 */
int
metaslab_alloc(spa_t *spa, uint64_t psize, dva_t *dva, uint64_t txg)
{
	metaslab_t *msp;
	metaslab_group_t *mg, *rotor;
	metaslab_class_t *mc;
	vdev_t *vd;
	uint64_t offset = -1ULL;
	uint64_t asize;

	mc = spa_metaslab_class_select(spa);

	/*
	 * Start at the rotor and loop through all mgs until we find something.
	 * Note that there's no locking on mc_rotor or mc_allocated because
	 * nothing actually breaks if we miss a few updates -- we just won't
	 * allocate quite as evenly.  It all balances out over time.
	 */
	mg = rotor = mc->mc_rotor;
	do {
		vd = mg->mg_vd;
		asize = vdev_psize_to_asize(vd, psize);
		ASSERT(P2PHASE(asize, 1ULL << vd->vdev_ashift) == 0);

		msp = metaslab_group_alloc(mg, asize, &offset, txg);
		if (msp != NULL) {
			ASSERT(offset != -1ULL);

			/*
			 * If we've just selected this metaslab group,
			 * figure out whether the corresponding vdev is
			 * over- or under-used relative to the pool,
			 * and set an allocation bias to even it out.
			 */
			if (mc->mc_allocated == 0) {
				vdev_stat_t *vs = &vd->vdev_stat;
				uint64_t alloc, space;
				int64_t vu, su;

				alloc = spa_get_alloc(spa);
				space = spa_get_space(spa);

				/*
				 * Determine percent used in units of 0..1024.
				 * (This is just to avoid floating point.)
				 */
				vu = (vs->vs_alloc << 10) / (vs->vs_space + 1);
				su = (alloc << 10) / (space + 1);

				/*
				 * Bias by at most +/- 25% of the aliquot.
				 */
				mg->mg_bias = ((su - vu) *
				    (int64_t)mg->mg_aliquot) / (1024 * 4);
			}

			if (atomic_add_64_nv(&mc->mc_allocated, asize) >=
			    mg->mg_aliquot + mg->mg_bias) {
				mc->mc_rotor = mg->mg_next;
				mc->mc_allocated = 0;
			}

			DVA_SET_VDEV(dva, vd->vdev_id);
			DVA_SET_OFFSET(dva, offset);
			DVA_SET_GANG(dva, 0);
			DVA_SET_ASIZE(dva, asize);

			return (0);
		}
		mc->mc_rotor = mg->mg_next;
		mc->mc_allocated = 0;
	} while ((mg = mg->mg_next) != rotor);

	DVA_SET_VDEV(dva, 0);
	DVA_SET_OFFSET(dva, 0);
	DVA_SET_GANG(dva, 0);

	return (ENOSPC);
}

/*
 * Free the block represented by DVA in the context of the specified
 * transaction group.
 */
void
metaslab_free(spa_t *spa, dva_t *dva, uint64_t txg, boolean_t now)
{
	uint64_t vdev = DVA_GET_VDEV(dva);
	uint64_t offset = DVA_GET_OFFSET(dva);
	uint64_t size = DVA_GET_ASIZE(dva);
	vdev_t *vd;
	metaslab_t *msp;

	if (txg > spa_freeze_txg(spa))
		return;

	if ((vd = vdev_lookup_top(spa, vdev)) == NULL) {
		cmn_err(CE_WARN, "metaslab_free(): bad vdev %llu",
		    (u_longlong_t)vdev);
		ASSERT(0);
		return;
	}

	if ((offset >> vd->vdev_ms_shift) >= vd->vdev_ms_count) {
		cmn_err(CE_WARN, "metaslab_free(): bad offset %llu",
		    (u_longlong_t)offset);
		ASSERT(0);
		return;
	}

	msp = vd->vdev_ms[offset >> vd->vdev_ms_shift];

	if (DVA_GET_GANG(dva))
		size = vdev_psize_to_asize(vd, SPA_GANGBLOCKSIZE);

	mutex_enter(&msp->ms_lock);

	if (now) {
		space_map_remove(&msp->ms_allocmap[txg & TXG_MASK],
		    offset, size);
		space_map_free(&msp->ms_map, offset, size);
	} else {
		if (msp->ms_freemap[txg & TXG_MASK].sm_space == 0)
			vdev_dirty(vd, VDD_METASLAB, msp, txg);
		space_map_add(&msp->ms_freemap[txg & TXG_MASK], offset, size);
	}

	mutex_exit(&msp->ms_lock);
}
