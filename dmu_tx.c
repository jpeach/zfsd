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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/dmu.h>
#include <sys/dmu_impl.h>
#include <sys/dbuf.h>
#include <sys/dmu_tx.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dataset.h> /* for dsl_dataset_block_freeable() */
#include <sys/dsl_dir.h> /* for dsl_dir_tempreserve_*() */
#include <sys/dsl_pool.h>
#include <sys/zap_impl.h>	/* for ZAP_BLOCK_SHIFT */
#include <sys/spa.h>
#include <sys/zfs_context.h>

#ifdef ZFS_DEBUG
int dmu_use_tx_debug_bufs = 1;
#endif

dmu_tx_t *
dmu_tx_create_ds(dsl_dir_t *dd)
{
	dmu_tx_t *tx = kmem_zalloc(sizeof (dmu_tx_t), KM_SLEEP);
	tx->tx_dir = dd;
	if (dd)
		tx->tx_pool = dd->dd_pool;
	list_create(&tx->tx_holds, sizeof (dmu_tx_hold_t),
	    offsetof(dmu_tx_hold_t, dth_node));
	refcount_create(&tx->tx_space_written);
	refcount_create(&tx->tx_space_freed);
	return (tx);
}

dmu_tx_t *
dmu_tx_create(objset_t *os)
{
	dmu_tx_t *tx = dmu_tx_create_ds(os->os->os_dsl_dataset->ds_dir);
	tx->tx_objset = os;
	return (tx);
}

dmu_tx_t *
dmu_tx_create_assigned(struct dsl_pool *dp, uint64_t txg)
{
	dmu_tx_t *tx = dmu_tx_create_ds(NULL);

	ASSERT3U(txg, <=, dp->dp_tx.tx_open_txg);
	tx->tx_pool = dp;
	tx->tx_txg = txg;
	tx->tx_anyobj = TRUE;

	return (tx);
}

int
dmu_tx_is_syncing(dmu_tx_t *tx)
{
	return (tx->tx_anyobj);
}

int
dmu_tx_private_ok(dmu_tx_t *tx)
{
	return (tx->tx_anyobj || tx->tx_privateobj);
}

static void
dmu_tx_hold_object_impl(dmu_tx_t *tx, objset_t *os, uint64_t object,
    enum dmu_tx_hold_type type, dmu_tx_hold_func_t func,
    uint64_t arg1, uint64_t arg2)
{
	dmu_tx_hold_t *dth;
	dnode_t *dn = NULL;

	if (object != DMU_NEW_OBJECT) {
		dn = dnode_hold(os->os, object, tx);

		if (tx->tx_txg != 0) {
			mutex_enter(&dn->dn_mtx);
			/*
			 * dn->dn_assigned_txg == tx->tx_txg doesn't pose a
			 * problem, but there's no way for it to happen (for
			 * now, at least).
			 */
			ASSERT(dn->dn_assigned_txg == 0);
			ASSERT(dn->dn_assigned_tx == NULL);
			dn->dn_assigned_txg = tx->tx_txg;
			dn->dn_assigned_tx = tx;
			(void) refcount_add(&dn->dn_tx_holds, tx);
			mutex_exit(&dn->dn_mtx);
		}
	}

	dth = kmem_zalloc(sizeof (dmu_tx_hold_t), KM_SLEEP);
	dth->dth_dnode = dn;
	dth->dth_type = type;
	dth->dth_func = func;
	dth->dth_arg1 = arg1;
	dth->dth_arg2 = arg2;
	/*
	 * XXX Investigate using a different data structure to keep
	 * track of dnodes in a tx.  Maybe array, since there will
	 * generally not be many entries?
	 */
	list_insert_tail(&tx->tx_holds, dth);
}

void
dmu_tx_add_new_object(dmu_tx_t *tx, objset_t *os, uint64_t object)
{
	/*
	 * If we're syncing, they can manipulate any object anyhow, and
	 * the hold on the dnode_t can cause problems.
	 */
	if (!dmu_tx_is_syncing(tx)) {
		dmu_tx_hold_object_impl(tx, os, object, THT_NEWOBJECT,
		    NULL, 0, 0);
	}
}

/* ARGSUSED */
static void
dmu_tx_count_write(dmu_tx_t *tx, dnode_t *dn, uint64_t off, uint64_t len)
{
	uint64_t start, end, space;
	int min_bs, max_bs, min_ibs, max_ibs, epbs, bits;

	if (len == 0)
		return;

	min_bs = SPA_MINBLOCKSHIFT;
	max_bs = SPA_MAXBLOCKSHIFT;
	min_ibs = DN_MIN_INDBLKSHIFT;
	max_ibs = DN_MAX_INDBLKSHIFT;

	/*
	 * If there's more than one block, the blocksize can't change,
	 * so we can make a more precise estimate.  Alternatively,
	 * if the dnode's ibs is larger than max_ibs, always use that.
	 * This ensures that if we reduce DN_MAX_INDBLKSHIFT,
	 * the code will still work correctly on existing pools.
	 */
	if (dn && (dn->dn_maxblkid != 0 || dn->dn_indblkshift > max_ibs)) {
		min_ibs = max_ibs = dn->dn_indblkshift;
		if (dn->dn_datablkshift != 0)
			min_bs = max_bs = dn->dn_datablkshift;
	}

	/*
	 * 'end' is the last thing we will access, not one past.
	 * This way we won't overflow when accessing the last byte.
	 */
	start = P2ALIGN(off, 1ULL << max_bs);
	end = P2ROUNDUP(off + len, 1ULL << max_bs) - 1;
	space = end - start + 1;

	start >>= min_bs;
	end >>= min_bs;

	epbs = min_ibs - SPA_BLKPTRSHIFT;

	/*
	 * The object contains at most 2^(64 - min_bs) blocks,
	 * and each indirect level maps 2^epbs.
	 */
	for (bits = 64 - min_bs; bits >= 0; bits -= epbs) {
		start >>= epbs;
		end >>= epbs;
		/*
		 * If we increase the number of levels of indirection,
		 * we'll need new blkid=0 indirect blocks.  If start == 0,
		 * we're already accounting for that blocks; and if end == 0,
		 * we can't increase the number of levels beyond that.
		 */
		if (start != 0 && end != 0)
			space += 1ULL << max_ibs;
		space += (end - start + 1) << max_ibs;
	}

	ASSERT(space < 2 * DMU_MAX_ACCESS);

	tx->tx_space_towrite += space;
}

static void
dmu_tx_count_dnode(dmu_tx_t *tx, dnode_t *dn)
{
	dnode_t *mdn = tx->tx_objset->os->os_meta_dnode;
	uint64_t object = dn ? dn->dn_object : DN_MAX_OBJECT - 1;
	uint64_t pre_write_space;

	ASSERT(object < DN_MAX_OBJECT);
	pre_write_space = tx->tx_space_towrite;
	dmu_tx_count_write(tx, mdn, object << DNODE_SHIFT, 1 << DNODE_SHIFT);
	if (dn && dn->dn_dbuf->db_blkptr &&
	    dsl_dataset_block_freeable(dn->dn_objset->os_dsl_dataset,
	    dn->dn_dbuf->db_blkptr->blk_birth, tx)) {
		tx->tx_space_tooverwrite +=
			tx->tx_space_towrite - pre_write_space;
		tx->tx_space_towrite = pre_write_space;
	}
}

/* ARGSUSED */
static void
dmu_tx_hold_write_impl(dmu_tx_t *tx, dnode_t *dn, uint64_t off, uint64_t len)
{
	dmu_tx_count_write(tx, dn, off, len);
	dmu_tx_count_dnode(tx, dn);
}

void
dmu_tx_hold_write(dmu_tx_t *tx, uint64_t object, uint64_t off, int len)
{
	ASSERT(tx->tx_txg == 0);
	ASSERT(len > 0 && len < DMU_MAX_ACCESS);
	ASSERT(UINT64_MAX - off >= len - 1);

	dmu_tx_hold_object_impl(tx, tx->tx_objset, object, THT_WRITE,
	    dmu_tx_hold_write_impl, off, len);
}

static void
dmu_tx_count_free(dmu_tx_t *tx, dnode_t *dn, uint64_t off, uint64_t len)
{
	uint64_t blkid, nblks;
	uint64_t space = 0;
	dsl_dataset_t *ds = dn->dn_objset->os_dsl_dataset;

	ASSERT(dn->dn_assigned_tx == tx || dn->dn_assigned_tx == NULL);

	if (dn->dn_datablkshift == 0)
		return;
	/*
	 * not that the dnode can change, since it isn't dirty, but
	 * dbuf_hold_impl() wants us to have the struct_rwlock.
	 * also need it to protect dn_maxblkid.
	 */
	rw_enter(&dn->dn_struct_rwlock, RW_READER);
	blkid = off >> dn->dn_datablkshift;
	nblks = (off + len) >> dn->dn_datablkshift;

	if (blkid >= dn->dn_maxblkid)
		goto out;
	if (blkid + nblks > dn->dn_maxblkid)
		nblks = dn->dn_maxblkid - blkid;

	/* don't bother after the 100,000 blocks */
	nblks = MIN(nblks, 128*1024);

	if (dn->dn_phys->dn_nlevels == 1) {
		int i;
		for (i = 0; i < nblks; i++) {
			blkptr_t *bp = dn->dn_phys->dn_blkptr;
			ASSERT3U(blkid + i, <, dn->dn_phys->dn_nblkptr);
			bp += blkid + i;
			if (dsl_dataset_block_freeable(ds, bp->blk_birth, tx)) {
				dprintf_bp(bp, "can free old%s", "");
				space += BP_GET_ASIZE(bp);
			}
		}
		goto out;
	}

	while (nblks) {
		dmu_buf_impl_t *dbuf;
		int err, epbs, blkoff, tochk;

		epbs = dn->dn_indblkshift - SPA_BLKPTRSHIFT;
		blkoff = P2PHASE(blkid, 1<<epbs);
		tochk = MIN((1<<epbs) - blkoff, nblks);

		err = dbuf_hold_impl(dn, 1, blkid >> epbs, TRUE, FTAG, &dbuf);
		if (err == 0) {
			int i;
			blkptr_t *bp;

			dbuf_read_havestruct(dbuf);

			bp = dbuf->db.db_data;
			bp += blkoff;

			for (i = 0; i < tochk; i++) {
				if (dsl_dataset_block_freeable(ds,
				    bp[i].blk_birth, tx)) {
					dprintf_bp(&bp[i],
					    "can free old%s", "");
					space += BP_GET_ASIZE(&bp[i]);
				}
			}
			dbuf_remove_ref(dbuf, FTAG);
		} else {
			/* the indirect block is sparse */
			ASSERT(err == ENOENT);
		}

		blkid += tochk;
		nblks -= tochk;
	}
out:
	rw_exit(&dn->dn_struct_rwlock);

	tx->tx_space_tofree += space;
}

static void
dmu_tx_hold_free_impl(dmu_tx_t *tx, dnode_t *dn, uint64_t off, uint64_t len)
{
	int dirty;

	/* first block */
	if (off != 0 /* || dn->dn_maxblkid == 0 */)
		dmu_tx_count_write(tx, dn, off, 1);
	/* last block */
	if (len != DMU_OBJECT_END)
		dmu_tx_count_write(tx, dn, off+len, 1);

	dmu_tx_count_dnode(tx, dn);

	if (off >= (dn->dn_maxblkid+1) * dn->dn_datablksz)
		return;
	if (len == DMU_OBJECT_END)
		len = (dn->dn_maxblkid+1) * dn->dn_datablksz - off;

	/* XXX locking */
	dirty = dn->dn_dirtyblksz[0] | dn->dn_dirtyblksz[1] |
	    dn->dn_dirtyblksz[2] | dn->dn_dirtyblksz[3];
	if (dn->dn_assigned_tx != NULL && !dirty)
		dmu_tx_count_free(tx, dn, off, len);
}

void
dmu_tx_hold_free(dmu_tx_t *tx, uint64_t object, uint64_t off, uint64_t len)
{
	ASSERT(tx->tx_txg == 0);

	dmu_tx_hold_object_impl(tx, tx->tx_objset, object, THT_FREE,
	    dmu_tx_hold_free_impl, off, len);
}

/* ARGSUSED */
static void
dmu_tx_hold_zap_impl(dmu_tx_t *tx, dnode_t *dn, uint64_t nops, uint64_t cops)
{
	uint64_t nblocks;
	int epbs;

	dmu_tx_count_dnode(tx, dn);

	if (dn == NULL) {
		/*
		 * Assuming that nops+cops is not super huge, we will be
		 * able to fit a new object's entries into one leaf
		 * block.  So there will be at most 2 blocks total,
		 * including the header block.
		 */
		dmu_tx_count_write(tx, dn, 0, 2 << ZAP_BLOCK_SHIFT);
		return;
	}

	ASSERT3P(dmu_ot[dn->dn_type].ot_byteswap, ==, zap_byteswap);

	if (dn->dn_maxblkid == 0 && nops == 0) {
		/*
		 * If there is only one block  (i.e. this is a micro-zap)
		 * and we are only doing updates, the accounting is simple.
		 */
		if (dsl_dataset_block_freeable(dn->dn_objset->os_dsl_dataset,
		    dn->dn_phys->dn_blkptr[0].blk_birth, tx))
			tx->tx_space_tooverwrite += dn->dn_datablksz;
		else
			tx->tx_space_towrite += dn->dn_datablksz;
		return;
	}

	/*
	 * 3 blocks overwritten per op: target leaf, ptrtbl block, header block
	 * 3 new blocks written per op: new split leaf, 2 grown ptrtbl blocks
	 */
	dmu_tx_count_write(tx, dn, dn->dn_maxblkid * dn->dn_datablksz,
	    (nops * 6ULL + cops * 3ULL) << ZAP_BLOCK_SHIFT);

	/*
	 * If the modified blocks are scattered to the four winds,
	 * we'll have to modify an indirect twig for each.
	 */
	epbs = dn->dn_indblkshift - SPA_BLKPTRSHIFT;
	for (nblocks = dn->dn_maxblkid >> epbs; nblocks != 0; nblocks >>= epbs)
		tx->tx_space_towrite +=
		    ((nops + cops) * 3ULL) << dn->dn_indblkshift;
}

void
dmu_tx_hold_zap(dmu_tx_t *tx, uint64_t object, int ops)
{
	ASSERT(tx->tx_txg == 0);

	dmu_tx_hold_object_impl(tx, tx->tx_objset, object, THT_ZAP,
	    dmu_tx_hold_zap_impl, (ops > 0?ops:0), (ops < 0?-ops:0));
}

void
dmu_tx_hold_bonus(dmu_tx_t *tx, uint64_t object)
{
	ASSERT(tx->tx_txg == 0);

	dmu_tx_hold_object_impl(tx, tx->tx_objset, object, THT_BONUS,
	    dmu_tx_hold_write_impl, 0, 0);
}


/* ARGSUSED */
static void
dmu_tx_hold_space_impl(dmu_tx_t *tx, dnode_t *dn,
    uint64_t space, uint64_t unused)
{
	tx->tx_space_towrite += space;
}

void
dmu_tx_hold_space(dmu_tx_t *tx, uint64_t space)
{
	ASSERT(tx->tx_txg == 0);

	dmu_tx_hold_object_impl(tx, tx->tx_objset, DMU_NEW_OBJECT, THT_SPACE,
	    dmu_tx_hold_space_impl, space, 0);
}

int
dmu_tx_holds(dmu_tx_t *tx, uint64_t object)
{
	dmu_tx_hold_t *dth;
	int holds = 0;

	/*
	 * By asserting that the tx is assigned, we're counting the
	 * number of dn_tx_holds, which is the same as the number of
	 * dn_holds.  Otherwise, we'd be counting dn_holds, but
	 * dn_tx_holds could be 0.
	 */
	ASSERT(tx->tx_txg != 0);

	/* if (tx->tx_anyobj == TRUE) */
		/* return (0); */

	for (dth = list_head(&tx->tx_holds); dth;
	    dth = list_next(&tx->tx_holds, dth)) {
		if (dth->dth_dnode && dth->dth_dnode->dn_object == object)
			holds++;
	}

	return (holds);
}

#ifdef ZFS_DEBUG
void
dmu_tx_dirty_buf(dmu_tx_t *tx, dmu_buf_impl_t *db)
{
	dmu_tx_hold_t *dth;
	int match_object = FALSE, match_offset = FALSE;
	dnode_t *dn = db->db_dnode;

	ASSERT(tx->tx_txg != 0);
	ASSERT(tx->tx_objset == NULL || dn->dn_objset == tx->tx_objset->os);
	ASSERT3U(dn->dn_object, ==, db->db.db_object);

	if (tx->tx_anyobj)
		return;

	/* XXX No checking on the meta dnode for now */
	if (db->db.db_object & DMU_PRIVATE_OBJECT)
		return;

	for (dth = list_head(&tx->tx_holds); dth;
	    dth = list_next(&tx->tx_holds, dth)) {
		ASSERT(dn == NULL || dn->dn_assigned_txg == tx->tx_txg);
		if (dth->dth_dnode == dn && dth->dth_type != THT_NEWOBJECT)
			match_object = TRUE;
		if (dth->dth_dnode == NULL || dth->dth_dnode == dn) {
			int datablkshift = dn->dn_datablkshift ?
			    dn->dn_datablkshift : SPA_MAXBLOCKSHIFT;
			int epbs = dn->dn_indblkshift - SPA_BLKPTRSHIFT;
			int shift = datablkshift + epbs * db->db_level;
			uint64_t beginblk = shift >= 64 ? 0 :
			    (dth->dth_arg1 >> shift);
			uint64_t endblk = shift >= 64 ? 0 :
			    ((dth->dth_arg1 + dth->dth_arg2 - 1) >> shift);
			uint64_t blkid = db->db_blkid;

			/* XXX dth_arg2 better not be zero... */

			dprintf("found dth type %x beginblk=%llx endblk=%llx\n",
			    dth->dth_type, beginblk, endblk);

			switch (dth->dth_type) {
			case THT_WRITE:
				if (blkid >= beginblk && blkid <= endblk)
					match_offset = TRUE;
				/*
				 * We will let this hold work for the bonus
				 * buffer so that we don't need to hold it
				 * when creating a new object.
				 */
				if (blkid == DB_BONUS_BLKID)
					match_offset = TRUE;
				/*
				 * They might have to increase nlevels,
				 * thus dirtying the new TLIBs.  Or the
				 * might have to change the block size,
				 * thus dirying the new lvl=0 blk=0.
				 */
				if (blkid == 0)
					match_offset = TRUE;
				break;
			case THT_FREE:
				if (blkid == beginblk &&
				    (dth->dth_arg1 != 0 ||
				    dn->dn_maxblkid == 0))
					match_offset = TRUE;
				if (blkid == endblk &&
				    dth->dth_arg2 != DMU_OBJECT_END)
					match_offset = TRUE;
				break;
			case THT_BONUS:
				if (blkid == DB_BONUS_BLKID)
					match_offset = TRUE;
				break;
			case THT_ZAP:
				match_offset = TRUE;
				break;
			case THT_NEWOBJECT:
				match_object = TRUE;
				break;
			default:
				ASSERT(!"bad dth_type");
			}
		}
		if (match_object && match_offset)
			return;
	}
	panic("dirtying dbuf obj=%llx lvl=%u blkid=%llx but not tx_held\n",
	    (u_longlong_t)db->db.db_object, db->db_level,
	    (u_longlong_t)db->db_blkid);
}
#endif

static int
dmu_tx_try_assign(dmu_tx_t *tx, uint64_t txg_how, dmu_tx_hold_t **last_dth)
{
	dmu_tx_hold_t *dth;
	uint64_t lsize, asize, fsize;

	*last_dth = NULL;

	tx->tx_space_towrite = 0;
	tx->tx_space_tofree = 0;
	tx->tx_space_tooverwrite = 0;
	tx->tx_txg = txg_hold_open(tx->tx_pool, &tx->tx_txgh);

	if (txg_how >= TXG_INITIAL && txg_how != tx->tx_txg)
		return (ERESTART);

	for (dth = list_head(&tx->tx_holds); dth;
	    *last_dth = dth, dth = list_next(&tx->tx_holds, dth)) {
		dnode_t *dn = dth->dth_dnode;
		if (dn != NULL) {
			mutex_enter(&dn->dn_mtx);
			while (dn->dn_assigned_txg == tx->tx_txg - 1) {
				if (txg_how != TXG_WAIT) {
					mutex_exit(&dn->dn_mtx);
					return (ERESTART);
				}
				cv_wait(&dn->dn_notxholds, &dn->dn_mtx);
			}
			if (dn->dn_assigned_txg == 0) {
				ASSERT(dn->dn_assigned_tx == NULL);
				dn->dn_assigned_txg = tx->tx_txg;
				dn->dn_assigned_tx = tx;
			} else {
				ASSERT(dn->dn_assigned_txg == tx->tx_txg);
				if (dn->dn_assigned_tx != tx)
					dn->dn_assigned_tx = NULL;
			}
			(void) refcount_add(&dn->dn_tx_holds, tx);
			mutex_exit(&dn->dn_mtx);
		}
		if (dth->dth_func)
			dth->dth_func(tx, dn, dth->dth_arg1, dth->dth_arg2);
	}

	/*
	 * Convert logical size to worst-case allocated size.
	 */
	fsize = spa_get_asize(tx->tx_pool->dp_spa, tx->tx_space_tooverwrite) +
	    tx->tx_space_tofree;
	lsize = tx->tx_space_towrite + tx->tx_space_tooverwrite;
	asize = spa_get_asize(tx->tx_pool->dp_spa, lsize);
	tx->tx_space_towrite = asize;

	if (tx->tx_dir && asize != 0) {
		int err = dsl_dir_tempreserve_space(tx->tx_dir,
		    lsize, asize, fsize, &tx->tx_tempreserve_cookie, tx);
		if (err)
			return (err);
	}

	return (0);
}

static uint64_t
dmu_tx_unassign(dmu_tx_t *tx, dmu_tx_hold_t *last_dth)
{
	uint64_t txg = tx->tx_txg;
	dmu_tx_hold_t *dth;

	ASSERT(txg != 0);

	txg_rele_to_quiesce(&tx->tx_txgh);

	for (dth = last_dth; dth; dth = list_prev(&tx->tx_holds, dth)) {
		dnode_t *dn = dth->dth_dnode;

		if (dn == NULL)
			continue;
		mutex_enter(&dn->dn_mtx);
		ASSERT3U(dn->dn_assigned_txg, ==, txg);

		if (refcount_remove(&dn->dn_tx_holds, tx) == 0) {
			dn->dn_assigned_txg = 0;
			dn->dn_assigned_tx = NULL;
			cv_broadcast(&dn->dn_notxholds);
		}
		mutex_exit(&dn->dn_mtx);
	}

	txg_rele_to_sync(&tx->tx_txgh);

	tx->tx_txg = 0;
	return (txg);
}

/*
 * Assign tx to a transaction group.  txg_how can be one of:
 *
 * (1)	TXG_WAIT.  If the current open txg is full, waits until there's
 *	a new one.  This should be used when you're not holding locks.
 *	If will only fail if we're truly out of space (or over quota).
 *
 * (2)	TXG_NOWAIT.  If we can't assign into the current open txg without
 *	blocking, returns immediately with ERESTART.  This should be used
 *	whenever you're holding locks.  On an ERESTART error, the caller
 *	should drop locks, do a txg_wait_open(dp, 0), and try again.
 *
 * (3)	A specific txg.  Use this if you need to ensure that multiple
 *	transactions all sync in the same txg.  Like TXG_NOWAIT, it
 *	returns ERESTART if it can't assign you into the requested txg.
 */
int
dmu_tx_assign(dmu_tx_t *tx, uint64_t txg_how)
{
	dmu_tx_hold_t *last_dth;
	int err;

	ASSERT(tx->tx_txg == 0);
	ASSERT(txg_how != 0);
	ASSERT(!dsl_pool_sync_context(tx->tx_pool));
	ASSERT3U(tx->tx_space_towrite, ==, 0);
	ASSERT3U(tx->tx_space_tofree, ==, 0);

	while ((err = dmu_tx_try_assign(tx, txg_how, &last_dth)) != 0) {
		uint64_t txg = dmu_tx_unassign(tx, last_dth);

		if (err != ERESTART || txg_how != TXG_WAIT)
			return (err);

		txg_wait_open(tx->tx_pool, txg + 1);
	}

	txg_rele_to_quiesce(&tx->tx_txgh);

	return (0);
}

void
dmu_tx_willuse_space(dmu_tx_t *tx, int64_t delta)
{
	if (tx->tx_dir == NULL || delta == 0)
		return;

	if (delta > 0) {
		ASSERT3U(refcount_count(&tx->tx_space_written) + delta, <=,
		    tx->tx_space_towrite);
		(void) refcount_add_many(&tx->tx_space_written, delta, NULL);
	} else {
		(void) refcount_add_many(&tx->tx_space_freed, -delta, NULL);
	}
}

void
dmu_tx_commit(dmu_tx_t *tx)
{
	dmu_tx_hold_t *dth;

	ASSERT(tx->tx_txg != 0);

	while (dth = list_head(&tx->tx_holds)) {
		dnode_t *dn = dth->dth_dnode;

		list_remove(&tx->tx_holds, dth);
		kmem_free(dth, sizeof (dmu_tx_hold_t));
		if (dn == NULL)
			continue;
		mutex_enter(&dn->dn_mtx);
		ASSERT3U(dn->dn_assigned_txg, ==, tx->tx_txg);

		if (refcount_remove(&dn->dn_tx_holds, tx) == 0) {
			dn->dn_assigned_txg = 0;
			dn->dn_assigned_tx = NULL;
			cv_broadcast(&dn->dn_notxholds);
		}
		mutex_exit(&dn->dn_mtx);
		dnode_rele(dn, tx);
	}

	if (tx->tx_dir && tx->tx_space_towrite > 0) {
		dsl_dir_tempreserve_clear(tx->tx_tempreserve_cookie, tx);
	}

	if (tx->tx_anyobj == FALSE)
		txg_rele_to_sync(&tx->tx_txgh);
	dprintf("towrite=%llu written=%llu tofree=%llu freed=%llu\n",
	    tx->tx_space_towrite, refcount_count(&tx->tx_space_written),
	    tx->tx_space_tofree, refcount_count(&tx->tx_space_freed));
	refcount_destroy_many(&tx->tx_space_written,
	    refcount_count(&tx->tx_space_written));
	refcount_destroy_many(&tx->tx_space_freed,
	    refcount_count(&tx->tx_space_freed));
#ifdef ZFS_DEBUG
	if (tx->tx_debug_buf)
		kmem_free(tx->tx_debug_buf, 4096);
#endif
	kmem_free(tx, sizeof (dmu_tx_t));
}

void
dmu_tx_abort(dmu_tx_t *tx)
{
	dmu_tx_hold_t *dth;

	ASSERT(tx->tx_txg == 0);

	while (dth = list_head(&tx->tx_holds)) {
		dnode_t *dn = dth->dth_dnode;

		list_remove(&tx->tx_holds, dth);
		kmem_free(dth, sizeof (dmu_tx_hold_t));
		if (dn != NULL)
			dnode_rele(dn, tx);
	}
	refcount_destroy_many(&tx->tx_space_written,
	    refcount_count(&tx->tx_space_written));
	refcount_destroy_many(&tx->tx_space_freed,
	    refcount_count(&tx->tx_space_freed));
#ifdef ZFS_DEBUG
	if (tx->tx_debug_buf)
		kmem_free(tx->tx_debug_buf, 4096);
#endif
	kmem_free(tx, sizeof (dmu_tx_t));
}

uint64_t
dmu_tx_get_txg(dmu_tx_t *tx)
{
	ASSERT(tx->tx_txg != 0);
	return (tx->tx_txg);
}
