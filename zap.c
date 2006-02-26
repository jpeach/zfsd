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


/*
 * This file contains the top half of the zfs directory structure
 * implementation. The bottom half is in zap_leaf.c.
 *
 * The zdir is an extendable hash data structure. There is a table of
 * pointers to buckets (zap_t->zd_data->zd_leafs). The buckets are
 * each a constant size and hold a variable number of directory entries.
 * The buckets (aka "leaf nodes") are implemented in zap_leaf.c.
 *
 * The pointer table holds a power of 2 number of pointers.
 * (1<<zap_t->zd_data->zd_phys->zd_prefix_len).  The bucket pointed to
 * by the pointer at index i in the table holds entries whose hash value
 * has a zd_prefix_len - bit prefix
 */

#include <sys/spa.h>
#include <sys/dmu.h>
#include <sys/zfs_context.h>
#include <sys/zap.h>
#include <sys/zap_impl.h>
#include <sys/zap_leaf.h>

#define	MIN_FREE(l) (ZAP_LEAF_NUMCHUNKS(l)*9/10)

int fzap_default_block_shift = 14; /* 16k blocksize */

static void zap_grow_ptrtbl(zap_t *zap, dmu_tx_t *tx);
static int zap_tryupgradedir(zap_t *zap, dmu_tx_t *tx);
static zap_leaf_t *zap_get_leaf_byblk(zap_t *zap, uint64_t blkid,
    dmu_tx_t *tx, krw_t lt);
static void zap_leaf_pageout(dmu_buf_t *db, void *vl);


void
fzap_byteswap(void *vbuf, size_t size)
{
	uint64_t block_type;

	block_type = *(uint64_t *)vbuf;

	switch (block_type) {
	case ZBT_LEAF:
	case BSWAP_64(ZBT_LEAF):
		zap_leaf_byteswap(vbuf, size);
		return;
	case ZBT_HEADER:
	case BSWAP_64(ZBT_HEADER):
	default:
		/* it's a ptrtbl block */
		byteswap_uint64_array(vbuf, size);
		return;
	}
}

void
fzap_upgrade(zap_t *zap, dmu_tx_t *tx)
{
	dmu_buf_t *db;
	zap_leaf_t *l;
	int i;
	zap_phys_t *zp;

	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	zap->zap_ismicro = FALSE;

	(void) dmu_buf_update_user(zap->zap_dbuf, zap, zap,
	    &zap->zap_f.zap_phys, zap_pageout);

	mutex_init(&zap->zap_f.zap_num_entries_mtx, 0, 0, 0);
	zap->zap_f.zap_block_shift = highbit(zap->zap_dbuf->db_size) - 1;

	zp = zap->zap_f.zap_phys;
	/*
	 * explicitly zero it since it might be coming from an
	 * initialized microzap
	 */
	bzero(zap->zap_dbuf->db_data, zap->zap_dbuf->db_size);
	zp->zap_block_type = ZBT_HEADER;
	zp->zap_magic = ZAP_MAGIC;

	zp->zap_ptrtbl.zt_shift = ZAP_EMBEDDED_PTRTBL_SHIFT(zap);

	zp->zap_freeblk = 2;		/* block 1 will be the first leaf */
	zp->zap_num_leafs = 1;
	zp->zap_num_entries = 0;
	zp->zap_salt = zap->zap_salt;

	/* block 1 will be the first leaf */
	for (i = 0; i < (1<<zp->zap_ptrtbl.zt_shift); i++)
		ZAP_EMBEDDED_PTRTBL_ENT(zap, i) = 1;

	/*
	 * set up block 1 - the first leaf
	 */
	db = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    1<<FZAP_BLOCK_SHIFT(zap));
	dmu_buf_will_dirty(db, tx);

	l = kmem_zalloc(sizeof (zap_leaf_t), KM_SLEEP);
	l->l_dbuf = db;
	l->l_phys = db->db_data;

	zap_leaf_init(l);

	kmem_free(l, sizeof (zap_leaf_t));
	dmu_buf_rele(db);
}

static int
zap_tryupgradedir(zap_t *zap, dmu_tx_t *tx)
{
	if (RW_WRITE_HELD(&zap->zap_rwlock))
		return (1);
	if (rw_tryupgrade(&zap->zap_rwlock)) {
		dmu_buf_will_dirty(zap->zap_dbuf, tx);
		return (1);
	}
	return (0);
}

/*
 * Generic routines for dealing with the pointer & cookie tables.
 */

static void
zap_table_grow(zap_t *zap, zap_table_phys_t *tbl,
    void (*transfer_func)(const uint64_t *src, uint64_t *dst, int n),
    dmu_tx_t *tx)
{
	uint64_t b, newblk;
	dmu_buf_t *db_old, *db_new;
	int bs = FZAP_BLOCK_SHIFT(zap);
	int hepb = 1<<(bs-4);
	/* hepb = half the number of entries in a block */

	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	ASSERT(tbl->zt_blk != 0);
	ASSERT(tbl->zt_numblks > 0);

	if (tbl->zt_nextblk != 0) {
		newblk = tbl->zt_nextblk;
	} else {
		newblk = zap_allocate_blocks(zap, tbl->zt_numblks * 2, tx);
		tbl->zt_nextblk = newblk;
		ASSERT3U(tbl->zt_blks_copied, ==, 0);
		dmu_prefetch(zap->zap_objset, zap->zap_object,
		    tbl->zt_blk << bs, tbl->zt_numblks << bs);
	}

	/*
	 * Copy the ptrtbl from the old to new location, leaving the odd
	 * entries blank as we go.
	 */

	b = tbl->zt_blks_copied;
	db_old = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (tbl->zt_blk + b) << bs);
	dmu_buf_read(db_old);

	/* first half of entries in old[b] go to new[2*b+0] */
	db_new = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (newblk + 2*b+0) << bs);
	dmu_buf_will_dirty(db_new, tx);
	transfer_func(db_old->db_data, db_new->db_data, hepb);
	dmu_buf_rele(db_new);

	/* second half of entries in old[b] go to new[2*b+1] */
	db_new = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (newblk + 2*b+1) << bs);
	dmu_buf_will_dirty(db_new, tx);
	transfer_func((uint64_t *)db_old->db_data + hepb,
	    db_new->db_data, hepb);
	dmu_buf_rele(db_new);

	dmu_buf_rele(db_old);

	tbl->zt_blks_copied++;

	dprintf("copied block %llu of %llu\n",
	    tbl->zt_blks_copied, tbl->zt_numblks);

	if (tbl->zt_blks_copied == tbl->zt_numblks) {
		dmu_free_range(zap->zap_objset, zap->zap_object,
		    tbl->zt_blk << bs, tbl->zt_numblks << bs, tx);

		tbl->zt_blk = newblk;
		tbl->zt_numblks *= 2;
		tbl->zt_shift++;
		tbl->zt_nextblk = 0;
		tbl->zt_blks_copied = 0;

		dprintf("finished; numblocks now %llu (%lluk entries)\n",
		    tbl->zt_numblks, 1<<(tbl->zt_shift-10));
	}
}

static uint64_t
zap_table_store(zap_t *zap, zap_table_phys_t *tbl, uint64_t idx, uint64_t val,
    dmu_tx_t *tx)
{
	uint64_t blk, off, oldval;
	dmu_buf_t *db;
	int bs = FZAP_BLOCK_SHIFT(zap);

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	ASSERT(tbl->zt_blk != 0);

	dprintf("storing %llx at index %llx\n", val, idx);

	blk = idx >> (bs-3);
	off = idx & ((1<<(bs-3))-1);

	db = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (tbl->zt_blk + blk) << bs);
	dmu_buf_will_dirty(db, tx);
	oldval = ((uint64_t *)db->db_data)[off];
	((uint64_t *)db->db_data)[off] = val;
	dmu_buf_rele(db);

	if (tbl->zt_nextblk != 0) {
		idx *= 2;
		blk = idx >> (bs-3);
		off = idx & ((1<<(bs-3))-1);

		db = dmu_buf_hold(zap->zap_objset, zap->zap_object,
		    (tbl->zt_nextblk + blk) << bs);
		dmu_buf_will_dirty(db, tx);
		((uint64_t *)db->db_data)[off] = val;
		((uint64_t *)db->db_data)[off+1] = val;
		dmu_buf_rele(db);
	}

	return (oldval);
}

static uint64_t
zap_table_load(zap_t *zap, zap_table_phys_t *tbl, uint64_t idx)
{
	uint64_t blk, off, val;
	dmu_buf_t *db;
	int bs = FZAP_BLOCK_SHIFT(zap);

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	blk = idx >> (bs-3);
	off = idx & ((1<<(bs-3))-1);

	db = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    (tbl->zt_blk + blk) << bs);
	dmu_buf_read(db);
	val = ((uint64_t *)db->db_data)[off];
	dmu_buf_rele(db);
	return (val);
}

/*
 * Routines for growing the ptrtbl.
 */

static void
zap_ptrtbl_transfer(const uint64_t *src, uint64_t *dst, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		uint64_t lb = src[i];
		dst[2*i+0] = lb;
		dst[2*i+1] = lb;
	}
}

static void
zap_grow_ptrtbl(zap_t *zap, dmu_tx_t *tx)
{
	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_shift == 32)
		return;

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks == 0) {
		/*
		 * We are outgrowing the "embedded" ptrtbl (the one
		 * stored in the header block).  Give it its own entire
		 * block, which will double the size of the ptrtbl.
		 */
		uint64_t newblk;
		dmu_buf_t *db_new;

		ASSERT3U(zap->zap_f.zap_phys->zap_ptrtbl.zt_shift, ==,
		    ZAP_EMBEDDED_PTRTBL_SHIFT(zap));
		ASSERT3U(zap->zap_f.zap_phys->zap_ptrtbl.zt_blk, ==, 0);

		newblk = zap_allocate_blocks(zap, 1, tx);
		db_new = dmu_buf_hold(zap->zap_objset, zap->zap_object,
		    newblk << FZAP_BLOCK_SHIFT(zap));

		dmu_buf_will_dirty(db_new, tx);
		zap_ptrtbl_transfer(&ZAP_EMBEDDED_PTRTBL_ENT(zap, 0),
		    db_new->db_data, 1 << ZAP_EMBEDDED_PTRTBL_SHIFT(zap));
		dmu_buf_rele(db_new);

		zap->zap_f.zap_phys->zap_ptrtbl.zt_blk = newblk;
		zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks = 1;
		zap->zap_f.zap_phys->zap_ptrtbl.zt_shift++;

		ASSERT3U(1ULL << zap->zap_f.zap_phys->zap_ptrtbl.zt_shift, ==,
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks <<
		    (FZAP_BLOCK_SHIFT(zap)-3));
	} else {
		zap_table_grow(zap, &zap->zap_f.zap_phys->zap_ptrtbl,
		    zap_ptrtbl_transfer, tx);
	}
}

static void
zap_increment_num_entries(zap_t *zap, int delta, dmu_tx_t *tx)
{
	dmu_buf_will_dirty(zap->zap_dbuf, tx);
	mutex_enter(&zap->zap_f.zap_num_entries_mtx);

	ASSERT(delta > 0 || zap->zap_f.zap_phys->zap_num_entries >= -delta);

	zap->zap_f.zap_phys->zap_num_entries += delta;

	mutex_exit(&zap->zap_f.zap_num_entries_mtx);
}

uint64_t
zap_allocate_blocks(zap_t *zap, int nblocks, dmu_tx_t *tx)
{
	uint64_t newblk;
	ASSERT(tx != NULL);
	if (!RW_WRITE_HELD(&zap->zap_rwlock)) {
		dmu_buf_will_dirty(zap->zap_dbuf, tx);
	}
	newblk = atomic_add_64_nv(&zap->zap_f.zap_phys->zap_freeblk, nblocks) -
	    nblocks;
	return (newblk);
}


/*
 * This function doesn't increment zap_num_leafs because it's used to
 * allocate a leaf chain, which doesn't count against zap_num_leafs.
 * The directory must be held exclusively for this tx.
 */
zap_leaf_t *
zap_create_leaf(zap_t *zap, dmu_tx_t *tx)
{
	void *winner;
	zap_leaf_t *l = kmem_alloc(sizeof (zap_leaf_t), KM_SLEEP);

	ASSERT(tx != NULL);
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	/* hence we already dirtied zap->zap_dbuf */

	rw_init(&l->l_rwlock, 0, 0, 0);
	rw_enter(&l->l_rwlock, RW_WRITER);
	l->l_blkid = zap_allocate_blocks(zap, 1, tx);
	l->l_next = NULL;
	l->l_dbuf = NULL;
	l->l_phys = NULL;

	l->l_dbuf = dmu_buf_hold(zap->zap_objset, zap->zap_object,
	    l->l_blkid << FZAP_BLOCK_SHIFT(zap));
	winner = dmu_buf_set_user(l->l_dbuf, l, &l->l_phys, zap_leaf_pageout);
	ASSERT(winner == NULL);
	dmu_buf_will_dirty(l->l_dbuf, tx);

	zap_leaf_init(l);

	return (l);
}

/* ARGSUSED */
void
zap_destroy_leaf(zap_t *zap, zap_leaf_t *l, dmu_tx_t *tx)
{
	/* uint64_t offset = l->l_blkid << ZAP_BLOCK_SHIFT; */
	rw_exit(&l->l_rwlock);
	dmu_buf_rele(l->l_dbuf);
	/* XXX there are still holds on this block, so we can't free it? */
	/* dmu_free_range(zap->zap_objset, zap->zap_object, */
	    /* offset,  1<<ZAP_BLOCK_SHIFT, tx); */
}

int
fzap_count(zap_t *zap, uint64_t *count)
{
	ASSERT(!zap->zap_ismicro);
	mutex_enter(&zap->zap_f.zap_num_entries_mtx); /* unnecessary */
	*count = zap->zap_f.zap_phys->zap_num_entries;
	mutex_exit(&zap->zap_f.zap_num_entries_mtx);
	return (0);
}

/*
 * Routines for obtaining zap_leaf_t's
 */

void
zap_put_leaf(zap_leaf_t *l)
{
	zap_leaf_t *nl = l->l_next;
	while (nl) {
		zap_leaf_t *nnl = nl->l_next;
		rw_exit(&nl->l_rwlock);
		dmu_buf_rele(nl->l_dbuf);
		nl = nnl;
	}
	rw_exit(&l->l_rwlock);
	dmu_buf_rele(l->l_dbuf);
}

_NOTE(ARGSUSED(0))
static void
zap_leaf_pageout(dmu_buf_t *db, void *vl)
{
	zap_leaf_t *l = vl;

	rw_destroy(&l->l_rwlock);
	kmem_free(l, sizeof (zap_leaf_t));
}

static zap_leaf_t *
zap_open_leaf(uint64_t blkid, dmu_buf_t *db)
{
	zap_leaf_t *l, *winner;

	ASSERT(blkid != 0);

	l = kmem_alloc(sizeof (zap_leaf_t), KM_SLEEP);
	rw_init(&l->l_rwlock, 0, 0, 0);
	rw_enter(&l->l_rwlock, RW_WRITER);
	l->l_blkid = blkid;
	l->l_bs = highbit(db->db_size)-1;
	l->l_next = NULL;
	l->l_dbuf = db;
	l->l_phys = NULL;

	winner = dmu_buf_set_user(db, l, &l->l_phys, zap_leaf_pageout);

	rw_exit(&l->l_rwlock);
	if (winner != NULL) {
		/* someone else set it first */
		zap_leaf_pageout(NULL, l);
		l = winner;
	}

	/*
	 * There should be more hash entries than there can be
	 * chunks to put in the hash table
	 */
	ASSERT3U(ZAP_LEAF_HASH_NUMENTRIES(l), >, ZAP_LEAF_NUMCHUNKS(l) / 3);

	/* The chunks should begin at the end of the hash table */
	ASSERT3P(&ZAP_LEAF_CHUNK(l, 0), ==,
	    &l->l_phys->l_hash[ZAP_LEAF_HASH_NUMENTRIES(l)]);

	/* The chunks should end at the end of the block */
	ASSERT3U((uintptr_t)&ZAP_LEAF_CHUNK(l, ZAP_LEAF_NUMCHUNKS(l)) -
	    (uintptr_t)l->l_phys, ==, l->l_dbuf->db_size);

	return (l);
}

static zap_leaf_t *
zap_get_leaf_byblk_impl(zap_t *zap, uint64_t blkid, dmu_tx_t *tx, krw_t lt)
{
	dmu_buf_t *db;
	zap_leaf_t *l;
	int bs = FZAP_BLOCK_SHIFT(zap);

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	db = dmu_buf_hold(zap->zap_objset, zap->zap_object, blkid << bs);

	ASSERT3U(db->db_object, ==, zap->zap_object);
	ASSERT3U(db->db_offset, ==, blkid << bs);
	ASSERT3U(db->db_size, ==, 1 << bs);
	ASSERT(blkid != 0);

	dmu_buf_read(db);
	l = dmu_buf_get_user(db);

	if (l == NULL)
		l = zap_open_leaf(blkid, db);

	rw_enter(&l->l_rwlock, lt);
	/*
	 * Must lock before dirtying, otherwise l->l_phys could change,
	 * causing ASSERT below to fail.
	 */
	if (lt == RW_WRITER)
		dmu_buf_will_dirty(db, tx);
	ASSERT3U(l->l_blkid, ==, blkid);
	ASSERT3P(l->l_dbuf, ==, db);
	ASSERT3P(l->l_phys, ==, l->l_dbuf->db_data);
	ASSERT3U(l->lh_block_type, ==, ZBT_LEAF);
	ASSERT3U(l->lh_magic, ==, ZAP_LEAF_MAGIC);

	return (l);
}

static zap_leaf_t *
zap_get_leaf_byblk(zap_t *zap, uint64_t blkid, dmu_tx_t *tx, krw_t lt)
{
	zap_leaf_t *l, *nl;

	l = zap_get_leaf_byblk_impl(zap, blkid, tx, lt);

	nl = l;
	while (nl->lh_next != 0) {
		zap_leaf_t *nnl;
		nnl = zap_get_leaf_byblk_impl(zap, nl->lh_next, tx, lt);
		nl->l_next = nnl;
		nl = nnl;
	}

	return (l);
}

static uint64_t
zap_idx_to_blk(zap_t *zap, uint64_t idx)
{
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks == 0) {
		ASSERT3U(idx, <,
		    (1ULL << zap->zap_f.zap_phys->zap_ptrtbl.zt_shift));
		return (ZAP_EMBEDDED_PTRTBL_ENT(zap, idx));
	} else {
		return (zap_table_load(zap, &zap->zap_f.zap_phys->zap_ptrtbl,
		    idx));
	}
}

static void
zap_set_idx_to_blk(zap_t *zap, uint64_t idx, uint64_t blk, dmu_tx_t *tx)
{
	ASSERT(tx != NULL);
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_blk == 0) {
		ZAP_EMBEDDED_PTRTBL_ENT(zap, idx) = blk;
	} else {
		(void) zap_table_store(zap, &zap->zap_f.zap_phys->zap_ptrtbl,
		    idx, blk, tx);
	}
}

static zap_leaf_t *
zap_deref_leaf(zap_t *zap, uint64_t h, dmu_tx_t *tx, krw_t lt)
{
	uint64_t idx;
	zap_leaf_t *l;

	ASSERT(zap->zap_dbuf == NULL ||
	    zap->zap_f.zap_phys == zap->zap_dbuf->db_data);
	ASSERT3U(zap->zap_f.zap_phys->zap_magic, ==, ZAP_MAGIC);
	idx = ZAP_HASH_IDX(h, zap->zap_f.zap_phys->zap_ptrtbl.zt_shift);
	l = zap_get_leaf_byblk(zap, zap_idx_to_blk(zap, idx), tx, lt);

	ASSERT3U(ZAP_HASH_IDX(h, l->lh_prefix_len), ==, l->lh_prefix);

	return (l);
}


static zap_leaf_t *
zap_expand_leaf(zap_t *zap, zap_leaf_t *l, uint64_t hash, dmu_tx_t *tx)
{
	zap_leaf_t *nl;
	int prefix_diff, i, err;
	uint64_t sibling;

	ASSERT3U(l->lh_prefix_len, <=,
	    zap->zap_f.zap_phys->zap_ptrtbl.zt_shift);
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	ASSERT3U(ZAP_HASH_IDX(hash, l->lh_prefix_len), ==, l->lh_prefix);

	if (zap_tryupgradedir(zap, tx) == 0) {
		/* failed to upgrade */
		int old_prefix_len = l->lh_prefix_len;
		objset_t *os = zap->zap_objset;
		uint64_t object = zap->zap_object;

		zap_put_leaf(l);
		zap_unlockdir(zap);
		err = zap_lockdir(os, object, tx, RW_WRITER, FALSE, &zap);
		ASSERT3U(err, ==, 0);
		ASSERT(!zap->zap_ismicro);
		l = zap_deref_leaf(zap, hash, tx, RW_WRITER);

		if (l->lh_prefix_len != old_prefix_len)
			/* it split while our locks were down */
			return (l);
	}
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

	if (l->lh_prefix_len == zap->zap_f.zap_phys->zap_ptrtbl.zt_shift) {
		/* There's only one pointer to us. Chain on another leaf blk. */
		(void) zap_leaf_chainmore(l, zap_create_leaf(zap, tx));
		dprintf("chaining leaf %x/%d\n", l->lh_prefix,
		    l->lh_prefix_len);
		return (l);
	}

	ASSERT3U(ZAP_HASH_IDX(hash, l->lh_prefix_len), ==, l->lh_prefix);

	/* There's more than one pointer to us. Split this leaf. */
	nl = zap_leaf_split(zap, l, tx);

	/* set sibling pointers */
	prefix_diff =
	    zap->zap_f.zap_phys->zap_ptrtbl.zt_shift - l->lh_prefix_len;
	sibling = (ZAP_HASH_IDX(hash, l->lh_prefix_len) | 1) << prefix_diff;
	for (i = 0; i < (1ULL<<prefix_diff); i++) {
		ASSERT3U(zap_idx_to_blk(zap, sibling+i), ==, l->l_blkid);
		zap_set_idx_to_blk(zap, sibling+i, nl->l_blkid, tx);
		/* dprintf("set %d to %u %x\n", sibling+i, nl->l_blkid, nl); */
	}

	zap->zap_f.zap_phys->zap_num_leafs++;

	if (hash & (1ULL << (64 - l->lh_prefix_len))) {
		/* we want the sibling */
		zap_put_leaf(l);
		l = nl;
	} else {
		zap_put_leaf(nl);
	}

	return (l);
}

static void
zap_put_leaf_maybe_grow_ptrtbl(zap_t *zap, zap_leaf_t *l, dmu_tx_t *tx)
{
	int shift, err;

again:
	shift = zap->zap_f.zap_phys->zap_ptrtbl.zt_shift;

	if (l->lh_prefix_len == shift &&
	    (l->l_next != NULL || l->lh_nfree < MIN_FREE(l))) {
		/* this leaf will soon make us grow the pointer table */

		if (zap_tryupgradedir(zap, tx) == 0) {
			objset_t *os = zap->zap_objset;
			uint64_t zapobj = zap->zap_object;
			uint64_t blkid = l->l_blkid;

			zap_put_leaf(l);
			zap_unlockdir(zap);
			err = zap_lockdir(os, zapobj, tx,
			    RW_WRITER, FALSE, &zap);
			ASSERT3U(err, ==, 0);
			l = zap_get_leaf_byblk(zap, blkid, tx, RW_READER);
			goto again;
		}

		zap_put_leaf(l);
		zap_grow_ptrtbl(zap, tx);
	} else {
		zap_put_leaf(l);
	}
}


static int
fzap_checksize(uint64_t integer_size, uint64_t num_integers)
{
	/* Only integer sizes supported by C */
	switch (integer_size) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return (EINVAL);
	}

	/* Make sure we won't overflow */
	if (integer_size * num_integers < num_integers)
		return (EINVAL);
	if (integer_size * num_integers > (1<<fzap_default_block_shift))
		return (EINVAL);

	return (0);
}

/*
 * Routines for maniplulating attributes.
 */
int
fzap_lookup(zap_t *zap, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf)
{
	zap_leaf_t *l;
	int err;
	uint64_t hash;
	zap_entry_handle_t zeh;

	err = fzap_checksize(integer_size, num_integers);
	if (err != 0)
		return (err);

	hash = zap_hash(zap, name);
	l = zap_deref_leaf(zap, hash, NULL, RW_READER);
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err != 0)
		goto out;
	err = zap_entry_read(&zeh, integer_size, num_integers, buf);
out:
	zap_put_leaf(l);
	return (err);
}

int
fzap_add_cd(zap_t *zap, const char *name,
    uint64_t integer_size, uint64_t num_integers,
    const void *val, uint32_t cd, dmu_tx_t *tx, zap_leaf_t **lp)
{
	zap_leaf_t *l;
	uint64_t hash;
	int err;
	zap_entry_handle_t zeh;

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	ASSERT(!zap->zap_ismicro);
	ASSERT(fzap_checksize(integer_size, num_integers) == 0);

	hash = zap_hash(zap, name);
	l = zap_deref_leaf(zap, hash, tx, RW_WRITER);
retry:
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err == 0) {
		err = EEXIST;
		goto out;
	}
	ASSERT(err == ENOENT);

	/* XXX If this leaf is chained, split it if we can. */
	err = zap_entry_create(l, name, hash, cd,
	    integer_size, num_integers, val, &zeh);

	if (err == 0) {
		zap_increment_num_entries(zap, 1, tx);
	} else if (err == EAGAIN) {
		l = zap_expand_leaf(zap, l, hash, tx);
		goto retry;
	}

out:
	if (lp)
		*lp = l;
	else
		zap_put_leaf(l);
	return (err);
}

int
fzap_add(zap_t *zap, const char *name,
    uint64_t integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx)
{
	int err;
	zap_leaf_t *l;

	err = fzap_checksize(integer_size, num_integers);
	if (err != 0)
		return (err);

	err = fzap_add_cd(zap, name, integer_size, num_integers,
	    val, ZAP_MAXCD, tx, &l);

	zap_put_leaf_maybe_grow_ptrtbl(zap, l, tx);
	return (err);
}

int
fzap_update(zap_t *zap, const char *name,
    int integer_size, uint64_t num_integers, const void *val, dmu_tx_t *tx)
{
	zap_leaf_t *l;
	uint64_t hash;
	int err, create;
	zap_entry_handle_t zeh;

	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	err = fzap_checksize(integer_size, num_integers);
	if (err != 0)
		return (err);

	hash = zap_hash(zap, name);
	l = zap_deref_leaf(zap, hash, tx, RW_WRITER);
retry:
	err = zap_leaf_lookup(l, name, hash, &zeh);
	create = (err == ENOENT);
	ASSERT(err == 0 || err == ENOENT);

	/* XXX If this leaf is chained, split it if we can. */

	if (create) {
		err = zap_entry_create(l, name, hash, ZAP_MAXCD,
		    integer_size, num_integers, val, &zeh);
		if (err == 0)
			zap_increment_num_entries(zap, 1, tx);
	} else {
		err = zap_entry_update(&zeh, integer_size, num_integers, val);
	}

	if (err == EAGAIN) {
		l = zap_expand_leaf(zap, l, hash, tx);
		goto retry;
	}

	zap_put_leaf_maybe_grow_ptrtbl(zap, l, tx);
	return (err);
}

int
fzap_length(zap_t *zap, const char *name,
    uint64_t *integer_size, uint64_t *num_integers)
{
	zap_leaf_t *l;
	int err;
	uint64_t hash;
	zap_entry_handle_t zeh;

	hash = zap_hash(zap, name);
	l = zap_deref_leaf(zap, hash, NULL, RW_READER);
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err != 0)
		goto out;

	if (integer_size)
		*integer_size = zeh.zeh_integer_size;
	if (num_integers)
		*num_integers = zeh.zeh_num_integers;
out:
	zap_put_leaf(l);
	return (err);
}

int
fzap_remove(zap_t *zap, const char *name, dmu_tx_t *tx)
{
	zap_leaf_t *l;
	uint64_t hash;
	int err;
	zap_entry_handle_t zeh;

	hash = zap_hash(zap, name);
	l = zap_deref_leaf(zap, hash, tx, RW_WRITER);
	err = zap_leaf_lookup(l, name, hash, &zeh);
	if (err == 0) {
		zap_entry_remove(&zeh);
		zap_increment_num_entries(zap, -1, tx);
	}
	zap_put_leaf(l);
	dprintf("fzap_remove: ds=%p obj=%llu name=%s err=%d\n",
	    zap->zap_objset, zap->zap_object, name, err);
	return (err);
}

int
zap_value_search(objset_t *os, uint64_t zapobj, uint64_t value, char *name)
{
	zap_cursor_t zc;
	zap_attribute_t *za;
	int err;

	za = kmem_alloc(sizeof (zap_attribute_t), KM_SLEEP);
	for (zap_cursor_init(&zc, os, zapobj);
	    (err = zap_cursor_retrieve(&zc, za)) == 0;
	    zap_cursor_advance(&zc)) {
		if (za->za_first_integer == value) {
			(void) strcpy(name, za->za_name);
			break;
		}
	}
	zap_cursor_fini(&zc);
	kmem_free(za, sizeof (zap_attribute_t));
	return (err);
}


/*
 * Routines for iterating over the attributes.
 */

int
fzap_cursor_retrieve(zap_t *zap, zap_cursor_t *zc, zap_attribute_t *za)
{
	int err = ENOENT;
	zap_entry_handle_t zeh;
	zap_leaf_t *l;

	/* retrieve the next entry at or after zc_hash/zc_cd */
	/* if no entry, return ENOENT */

	if (zc->zc_leaf &&
	    (ZAP_HASH_IDX(zc->zc_hash, zc->zc_leaf->lh_prefix_len) !=
	    zc->zc_leaf->lh_prefix)) {
		rw_enter(&zc->zc_leaf->l_rwlock, RW_READER);
		zap_put_leaf(zc->zc_leaf);
		zc->zc_leaf = NULL;
	}

again:
	if (zc->zc_leaf == NULL) {
		zc->zc_leaf = zap_deref_leaf(zap, zc->zc_hash, NULL, RW_READER);
	} else {
		rw_enter(&zc->zc_leaf->l_rwlock, RW_READER);
	}
	l = zc->zc_leaf;

	err = zap_leaf_lookup_closest(l, zc->zc_hash, zc->zc_cd, &zeh);

	if (err == ENOENT) {
		uint64_t nocare = (1ULL << (64 - l->lh_prefix_len)) - 1;
		zc->zc_hash = (zc->zc_hash & ~nocare) + nocare + 1;
		zc->zc_cd = 0;
		if (l->lh_prefix_len == 0 || zc->zc_hash == 0) {
			zc->zc_hash = -1ULL;
		} else {
			zap_put_leaf(zc->zc_leaf);
			zc->zc_leaf = NULL;
			goto again;
		}
	}

	if (err == 0) {
		zc->zc_hash = zeh.zeh_hash;
		zc->zc_cd = zeh.zeh_cd;
		za->za_integer_length = zeh.zeh_integer_size;
		za->za_num_integers = zeh.zeh_num_integers;
		if (zeh.zeh_num_integers == 0) {
			za->za_first_integer = 0;
		} else {
			err = zap_entry_read(&zeh, 8, 1, &za->za_first_integer);
			ASSERT(err == 0 || err == EOVERFLOW);
		}
		err = zap_entry_read_name(&zeh,
		    sizeof (za->za_name), za->za_name);
		ASSERT(err == 0);
	}
	rw_exit(&zc->zc_leaf->l_rwlock);
	return (err);
}


static void
zap_stats_ptrtbl(zap_t *zap, uint64_t *tbl, int len, zap_stats_t *zs)
{
	int i;
	uint64_t lastblk = 0;

	/*
	 * NB: if a leaf has more pointers than an entire ptrtbl block
	 * can hold, then it'll be accounted for more than once, since
	 * we won't have lastblk.
	 */
	for (i = 0; i < len; i++) {
		zap_leaf_t *l;

		if (tbl[i] == lastblk)
			continue;
		lastblk = tbl[i];

		l = zap_get_leaf_byblk(zap, tbl[i], NULL, RW_READER);

		zap_stats_leaf(zap, l, zs);
		zap_put_leaf(l);
	}
}

void
fzap_get_stats(zap_t *zap, zap_stats_t *zs)
{
	int bs = FZAP_BLOCK_SHIFT(zap);
	zs->zs_ptrtbl_len = 1ULL << zap->zap_f.zap_phys->zap_ptrtbl.zt_shift;
	zs->zs_blocksize = 1ULL << bs;
	zs->zs_num_leafs = zap->zap_f.zap_phys->zap_num_leafs;
	zs->zs_num_entries = zap->zap_f.zap_phys->zap_num_entries;
	zs->zs_num_blocks = zap->zap_f.zap_phys->zap_freeblk;

	if (zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks == 0) {
		/* the ptrtbl is entirely in the header block. */
		zap_stats_ptrtbl(zap, &ZAP_EMBEDDED_PTRTBL_ENT(zap, 0),
		    1 << ZAP_EMBEDDED_PTRTBL_SHIFT(zap), zs);
	} else {
		int b;

		dmu_prefetch(zap->zap_objset, zap->zap_object,
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_blk << bs,
		    zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks << bs);

		for (b = 0; b < zap->zap_f.zap_phys->zap_ptrtbl.zt_numblks;
		    b++) {
			dmu_buf_t *db;

			db = dmu_buf_hold(zap->zap_objset, zap->zap_object,
			    (zap->zap_f.zap_phys->zap_ptrtbl.zt_blk + b) << bs);
			dmu_buf_read(db);
			zap_stats_ptrtbl(zap, db->db_data, 1<<(bs-3), zs);
			dmu_buf_rele(db);
		}
	}
}
