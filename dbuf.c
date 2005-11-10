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

#include <sys/zfs_context.h>
#include <sys/dmu.h>
#include <sys/dmu_impl.h>
#include <sys/dbuf.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dmu_tx.h>
#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/dmu_zfetch.h>

static void dbuf_destroy(dmu_buf_impl_t *db);
static int dbuf_undirty(dmu_buf_impl_t *db, dmu_tx_t *tx);
static arc_done_func_t dbuf_write_done;

/*
 * Global data structures and functions for the dbuf cache.
 */
taskq_t *dbuf_tq;
static kmem_cache_t *dbuf_cache;

/* ARGSUSED */
static int
dbuf_cons(void *vdb, void *unused, int kmflag)
{
	dmu_buf_impl_t *db = vdb;
	bzero(db, sizeof (dmu_buf_impl_t));

	mutex_init(&db->db_mtx, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&db->db_changed, NULL, CV_DEFAULT, NULL);
	refcount_create(&db->db_holds);
	return (0);
}

/* ARGSUSED */
static void
dbuf_dest(void *vdb, void *unused)
{
	dmu_buf_impl_t *db = vdb;
	mutex_destroy(&db->db_mtx);
	cv_destroy(&db->db_changed);
	refcount_destroy(&db->db_holds);
}

/*
 * dbuf hash table routines
 */
static dbuf_hash_table_t dbuf_hash_table;

static uint64_t dbuf_hash_count;

static uint64_t
dbuf_hash(void *os, uint64_t obj, uint8_t lvl, uint64_t blkid)
{
	uintptr_t osv = (uintptr_t)os;
	uint64_t crc = -1ULL;

	ASSERT(zfs_crc64_table[128] == ZFS_CRC64_POLY);
	crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ (lvl)) & 0xFF];
	crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ (osv >> 6)) & 0xFF];
	crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ (obj >> 0)) & 0xFF];
	crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ (obj >> 8)) & 0xFF];
	crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ (blkid >> 0)) & 0xFF];
	crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ (blkid >> 8)) & 0xFF];

	crc ^= (osv>>14) ^ (obj>>16) ^ (blkid>>16);

	return (crc);
}

#define	DBUF_HASH(os, obj, level, blkid) dbuf_hash(os, obj, level, blkid);

#define	DBUF_EQUAL(dbuf, os, obj, level, blkid)		\
	((dbuf)->db.db_object == (obj) &&		\
	(dbuf)->db_objset == (os) &&			\
	(dbuf)->db_level == (level) &&			\
	(dbuf)->db_blkid == (blkid))

dmu_buf_impl_t *
dbuf_find(dnode_t *dn, uint8_t level, uint64_t blkid)
{
	dbuf_hash_table_t *h = &dbuf_hash_table;
	objset_impl_t *os = dn->dn_objset;
	uint64_t obj = dn->dn_object;
	uint64_t hv = DBUF_HASH(os, obj, level, blkid);
	uint64_t idx = hv & h->hash_table_mask;
	dmu_buf_impl_t *db;

	mutex_enter(DBUF_HASH_MUTEX(h, idx));
	for (db = h->hash_table[idx]; db != NULL; db = db->db_hash_next) {
		if (DBUF_EQUAL(db, os, obj, level, blkid)) {
			mutex_enter(&db->db_mtx);
			if (!refcount_is_zero(&db->db_holds)) {
				mutex_exit(DBUF_HASH_MUTEX(h, idx));
				return (db);
			}
			mutex_exit(&db->db_mtx);
		}
	}
	mutex_exit(DBUF_HASH_MUTEX(h, idx));
	return (NULL);
}

/*
 * Insert an entry into the hash table.  If there is already an element
 * equal to elem in the hash table, then the already existing element
 * will be returned and the new element will not be inserted.
 * Otherwise returns NULL.
 */
static dmu_buf_impl_t *
dbuf_hash_insert(dmu_buf_impl_t *db)
{
	dbuf_hash_table_t *h = &dbuf_hash_table;
	objset_impl_t *os = db->db_objset;
	uint64_t obj = db->db.db_object;
	int level = db->db_level;
	uint64_t blkid = db->db_blkid;
	uint64_t hv = DBUF_HASH(os, obj, level, blkid);
	uint64_t idx = hv & h->hash_table_mask;
	dmu_buf_impl_t *dbf;

	mutex_enter(DBUF_HASH_MUTEX(h, idx));
	for (dbf = h->hash_table[idx]; dbf != NULL; dbf = dbf->db_hash_next) {
		if (DBUF_EQUAL(dbf, os, obj, level, blkid)) {
			mutex_enter(&dbf->db_mtx);
			if (!refcount_is_zero(&dbf->db_holds)) {
				mutex_exit(DBUF_HASH_MUTEX(h, idx));
				return (dbf);
			}
			mutex_exit(&dbf->db_mtx);
		}
	}

	mutex_enter(&db->db_mtx);
	db->db_hash_next = h->hash_table[idx];
	h->hash_table[idx] = db;
	mutex_exit(DBUF_HASH_MUTEX(h, idx));
	atomic_add_64(&dbuf_hash_count, 1);

	return (NULL);
}

/*
 * Remove an entry from the hash table.  This operation will
 * fail if there are any existing holds on the db.
 */
static void
dbuf_hash_remove(dmu_buf_impl_t *db)
{
	dbuf_hash_table_t *h = &dbuf_hash_table;
	uint64_t hv = DBUF_HASH(db->db_objset, db->db.db_object,
	    db->db_level, db->db_blkid);
	uint64_t idx = hv & h->hash_table_mask;
	dmu_buf_impl_t *dbf, **dbp;

	/*
	 * We musn't hold db_mtx to maintin lock ordering:
	 * DBUF_HASH_MUTEX > db_mtx.
	 */
	ASSERT(refcount_is_zero(&db->db_holds));
	ASSERT(db->db_dnode != NULL);
	ASSERT(!MUTEX_HELD(&db->db_mtx));

	mutex_enter(DBUF_HASH_MUTEX(h, idx));
	dbp = &h->hash_table[idx];
	while ((dbf = *dbp) != db) {
		dbp = &dbf->db_hash_next;
		ASSERT(dbf != NULL);
	}
	*dbp = db->db_hash_next;
	db->db_hash_next = NULL;
	mutex_exit(DBUF_HASH_MUTEX(h, idx));
	atomic_add_64(&dbuf_hash_count, -1);
}

static int dbuf_evictable(dmu_buf_impl_t *db);
static void dbuf_clear(dmu_buf_impl_t *db);

void
dbuf_evict(dmu_buf_impl_t *db)
{
	int err;

	ASSERT(MUTEX_HELD(&db->db_mtx));
	err = dbuf_evictable(db);
	ASSERT(err == TRUE);
	dbuf_clear(db);
	dbuf_destroy(db);
}

static void
dbuf_evict_user(dmu_buf_impl_t *db)
{
	ASSERT(MUTEX_HELD(&db->db_mtx));

	if (db->db_level != 0 || db->db_d.db_evict_func == NULL)
		return;

	if (db->db_d.db_user_data_ptr_ptr)
		*db->db_d.db_user_data_ptr_ptr = db->db.db_data;
	db->db_d.db_evict_func(&db->db, db->db_d.db_user_ptr);
	db->db_d.db_user_ptr = NULL;
	db->db_d.db_user_data_ptr_ptr = NULL;
	db->db_d.db_evict_func = NULL;
}

void
dbuf_init(void)
{
	uint64_t hsize = 1;
	dbuf_hash_table_t *h = &dbuf_hash_table;
	int i;

	/*
	 * The hash table is big enough to fill all of physical memory
	 * with an average 64k block size.  The table will take up
	 * totalmem*sizeof(void*)/64k bytes (i.e. 128KB/GB with 8-byte
	 * pointers).
	 */
	while (hsize * 65536 < physmem * PAGESIZE)
		hsize <<= 1;

	h->hash_table_mask = hsize - 1;
	h->hash_table = kmem_zalloc(hsize * sizeof (void *), KM_SLEEP);

	dbuf_cache = kmem_cache_create("dmu_buf_impl_t",
	    sizeof (dmu_buf_impl_t),
	    0, dbuf_cons, dbuf_dest, NULL, NULL, NULL, 0);
	dbuf_tq = taskq_create("dbuf_tq", 8, maxclsyspri, 50, INT_MAX,
	    TASKQ_PREPOPULATE);

	for (i = 0; i < DBUF_MUTEXES; i++)
		mutex_init(&h->hash_mutexes[i], NULL, MUTEX_DEFAULT, NULL);
}

void
dbuf_fini(void)
{
	dbuf_hash_table_t *h = &dbuf_hash_table;
	int i;

	taskq_destroy(dbuf_tq);
	dbuf_tq = NULL;

	for (i = 0; i < DBUF_MUTEXES; i++)
		mutex_destroy(&h->hash_mutexes[i]);
	kmem_free(h->hash_table, (h->hash_table_mask + 1) * sizeof (void *));
	kmem_cache_destroy(dbuf_cache);
}

/*
 * Other stuff.
 */

#ifdef ZFS_DEBUG
static void
dbuf_verify(dmu_buf_impl_t *db)
{
	int i;
	dnode_t *dn = db->db_dnode;

	ASSERT(MUTEX_HELD(&db->db_mtx));

	if (!(zfs_flags & ZFS_DEBUG_DBUF_VERIFY))
		return;

	ASSERT(db->db_objset != NULL);
	if (dn == NULL) {
		ASSERT(db->db_parent == NULL);
		ASSERT(db->db_blkptr == NULL);
	} else {
		ASSERT3U(db->db.db_object, ==, dn->dn_object);
		ASSERT3P(db->db_objset, ==, dn->dn_objset);
		ASSERT(list_head(&dn->dn_dbufs));
		ASSERT3U(db->db_level, <, dn->dn_nlevels);
	}
	if (db->db_blkid == DB_BONUS_BLKID) {
		ASSERT(dn != NULL);
		ASSERT3U(db->db.db_size, ==, dn->dn_bonuslen);
		ASSERT3U(db->db.db_offset, ==, DB_BONUS_BLKID);
	} else {
		ASSERT3U(db->db.db_offset, ==, db->db_blkid * db->db.db_size);
	}

	if (db->db_level == 0) {
		void **udpp = db->db_d.db_user_data_ptr_ptr;
		/* we can be momentarily larger in dnode_set_blksz() */
		if (db->db_blkid != DB_BONUS_BLKID && dn) {
			ASSERT3U(db->db.db_size, >=, dn->dn_datablksz);
		}
		if (udpp) {
			ASSERT((refcount_is_zero(&db->db_holds) &&
			    *udpp == NULL) ||
			    (!refcount_is_zero(&db->db_holds) &&
			    *udpp == db->db.db_data));
		}

		if (IS_DNODE_DNODE(db->db.db_object)) {
			for (i = 0; i < TXG_SIZE; i++) {
				/*
				 * it should only be modified in syncing
				 * context, so make sure we only have
				 * one copy of the data.
				 */
				ASSERT(db->db_d.db_data_old[i] == NULL ||
				    db->db_d.db_data_old[i] == db->db_buf);
			}
		}
	}

	/* verify db->db_blkptr */
	if (db->db_blkptr) {
		if (db->db_parent == dn->dn_dbuf) {
			/* db is pointed to by the dnode */
			/* ASSERT3U(db->db_blkid, <, dn->dn_nblkptr); */
			if (IS_DNODE_DNODE(db->db.db_object))
				ASSERT(db->db_parent == NULL);
			else
				ASSERT(db->db_parent != NULL);
			ASSERT3P(db->db_blkptr, ==,
			    &dn->dn_phys->dn_blkptr[db->db_blkid]);
		} else {
			/* db is pointed to by an indirect block */
			int epb = db->db_parent->db.db_size >> SPA_BLKPTRSHIFT;
			ASSERT3U(db->db_parent->db_level, ==, db->db_level+1);
			ASSERT3U(db->db_parent->db.db_object, ==,
			    db->db.db_object);
			/*
			 * dnode_grow_indblksz() can make this fail if we don't
			 * have the struct_rwlock.  XXX indblksz no longer
			 * grows.  safe to do this now?
			 */
			if (RW_WRITE_HELD(&db->db_dnode->dn_struct_rwlock)) {
				ASSERT3P(db->db_blkptr, ==,
				    ((blkptr_t *)db->db_parent->db.db_data +
				    db->db_blkid % epb));
			}
		}
	}
	if ((db->db_blkptr == NULL || BP_IS_HOLE(db->db_blkptr)) &&
	    db->db.db_data && db->db_blkid != DB_BONUS_BLKID &&
	    db->db_state != DB_FILL && !dn->dn_free_txg) {
		/*
		 * If the blkptr isn't set but they have nonzero data,
		 * it had better be dirty, otherwise we'll lose that
		 * data when we evict this buffer.
		 */
		if (db->db_dirtycnt == 0) {
			uint64_t *buf = db->db.db_data;
			int i;

			for (i = 0; i < db->db.db_size >> 3; i++) {
				ASSERT(buf[i] == 0);
			}
		}
	}
}
#endif

static void
dbuf_update_data(dmu_buf_impl_t *db)
{
	ASSERT(MUTEX_HELD(&db->db_mtx));
	if (db->db_level == 0 && db->db_d.db_user_data_ptr_ptr) {
		ASSERT(!refcount_is_zero(&db->db_holds));
		*db->db_d.db_user_data_ptr_ptr = db->db.db_data;
	}
}

static void
dbuf_set_data(dmu_buf_impl_t *db, arc_buf_t *buf)
{
	ASSERT(MUTEX_HELD(&db->db_mtx));
	ASSERT(buf->b_data != NULL);
	db->db_buf = buf;
	db->db.db_data = buf->b_data;
	dbuf_update_data(db);
}

uint64_t
dbuf_whichblock(dnode_t *dn, uint64_t offset)
{
	if (dn->dn_datablkshift) {
		return (offset >> dn->dn_datablkshift);
	} else {
		ASSERT3U(offset, <, dn->dn_datablksz);
		return (0);
	}
}

static void
dbuf_read_done(zio_t *zio, arc_buf_t *buf, void *vdb)
{
	dmu_buf_impl_t *db = vdb;

	mutex_enter(&db->db_mtx);
	ASSERT3U(db->db_state, ==, DB_READ);
	/*
	 * All reads are synchronous, so we must have a hold on the dbuf
	 */
	ASSERT(refcount_count(&db->db_holds) > 0);
	ASSERT(db->db.db_data == NULL);
	if (db->db_level == 0 && db->db_d.db_freed_in_flight) {
		/* we were freed in flight; disregard any error */
		arc_release(buf, db);
		bzero(buf->b_data, db->db.db_size);
		db->db_d.db_freed_in_flight = FALSE;
		dbuf_set_data(db, buf);
		db->db_state = DB_CACHED;
	} else if (zio == NULL || zio->io_error == 0) {
		dbuf_set_data(db, buf);
		db->db_state = DB_CACHED;
	} else {
		ASSERT(db->db_blkid != DB_BONUS_BLKID);
		arc_buf_free(buf, db);
		db->db_state = DB_UNCACHED;
		ASSERT3P(db->db_buf, ==, NULL);
	}
	cv_broadcast(&db->db_changed);
	mutex_exit(&db->db_mtx);
}

void
dbuf_read_impl(dmu_buf_impl_t *db, zio_t *zio, uint32_t flags)
{
	arc_buf_t *buf;
	blkptr_t *bp;

	ASSERT(!refcount_is_zero(&db->db_holds));
	/* We need the struct_rwlock to prevent db_blkptr from changing. */
	ASSERT(RW_LOCK_HELD(&db->db_dnode->dn_struct_rwlock));

	/*
	 * prefetch only data blocks (level 0) -- don't prefetch indirect
	 * blocks
	 */
	if ((db->db_level > 0) || (db->db_blkid == DB_BONUS_BLKID)) {
		flags |= DB_RF_NOPREFETCH;
	}

	if (((flags & DB_RF_NOPREFETCH) == 0) && (db->db_dnode != NULL)) {
		dmu_zfetch(&db->db_dnode->dn_zfetch, db->db.db_offset,
		    db->db.db_size);
	}

	if (db->db_state == DB_CACHED) {
		ASSERT(db->db.db_data != NULL);
		return;
	}

	mutex_enter(&db->db_mtx);

	if (db->db_state != DB_UNCACHED) {
		mutex_exit(&db->db_mtx);
		return;
	}

	ASSERT3U(db->db_state, ==, DB_UNCACHED);

	if (db->db_blkid == DB_BONUS_BLKID) {
		ASSERT3U(db->db_dnode->dn_bonuslen, ==, db->db.db_size);
		buf = arc_buf_alloc(db->db_dnode->dn_objset->os_spa,
		    DN_MAX_BONUSLEN, db);
		if (db->db.db_size < DN_MAX_BONUSLEN)
			bzero(buf->b_data, DN_MAX_BONUSLEN);
		bcopy(DN_BONUS(db->db_dnode->dn_phys), buf->b_data,
		    db->db.db_size);
		dbuf_set_data(db, buf);
		db->db_state = DB_CACHED;
		mutex_exit(&db->db_mtx);
		return;
	}

	if (db->db_level == 0 && dnode_block_freed(db->db_dnode, db->db_blkid))
		bp = NULL;
	else
		bp = db->db_blkptr;

	if (bp == NULL)
		dprintf_dbuf(db, "blkptr: %s\n", "NULL");
	else
		dprintf_dbuf_bp(db, bp, "%s", "blkptr:");

	if (bp == NULL || BP_IS_HOLE(bp)) {
		ASSERT(bp == NULL || BP_IS_HOLE(bp));
		dbuf_set_data(db, arc_buf_alloc(db->db_dnode->dn_objset->os_spa,
		    db->db.db_size, db));
		bzero(db->db.db_data, db->db.db_size);
		db->db_state = DB_CACHED;
		mutex_exit(&db->db_mtx);
		return;
	}

	db->db_state = DB_READ;
	mutex_exit(&db->db_mtx);

	/* ZIO_FLAG_CANFAIL callers have to check the parent zio's error */
	(void) arc_read(zio, db->db_dnode->dn_objset->os_spa, bp,
	    db->db_level > 0 ? byteswap_uint64_array :
	    dmu_ot[db->db_dnode->dn_type].ot_byteswap,
	    dbuf_read_done, db, ZIO_PRIORITY_SYNC_READ,
	    (flags & DB_RF_CANFAIL) ? ZIO_FLAG_CANFAIL : ZIO_FLAG_MUSTSUCCEED,
	    ARC_NOWAIT);
}

static int
dbuf_read_generic(dmu_buf_impl_t *db, uint32_t flags)
{
	zio_t *zio;
	int err;

	/*
	 * We don't have to hold the mutex to check db_state because it
	 * can't be freed while we have a hold on the buffer.
	 */
	ASSERT(!refcount_is_zero(&db->db_holds));
	if (db->db_state == DB_CACHED)
		return (0);

	if (db->db_state == DB_UNCACHED) {
		zio = zio_root(db->db_dnode->dn_objset->os_spa, NULL, NULL,
		    ZIO_FLAG_CANFAIL);
		if ((flags & DB_RF_HAVESTRUCT) == 0)
			rw_enter(&db->db_dnode->dn_struct_rwlock, RW_READER);
		dbuf_read_impl(db, zio, flags);
		if ((flags & DB_RF_HAVESTRUCT) == 0)
			rw_exit(&db->db_dnode->dn_struct_rwlock);
		err = zio_wait(zio);
		if (err)
			return (err);
	}

	mutex_enter(&db->db_mtx);
	while (db->db_state == DB_READ || db->db_state == DB_FILL) {
		ASSERT(db->db_state == DB_READ ||
		    (flags & DB_RF_HAVESTRUCT) == 0);
		cv_wait(&db->db_changed, &db->db_mtx);
	}
	ASSERT3U(db->db_state, ==, DB_CACHED);
	mutex_exit(&db->db_mtx);

	return (0);
}

#pragma weak dmu_buf_read = dbuf_read
void
dbuf_read(dmu_buf_impl_t *db)
{
	int err;

	err = dbuf_read_generic(db, DB_RF_MUST_SUCCEED);
	ASSERT(err == 0);
}

#pragma weak dmu_buf_read_canfail = dbuf_read_canfail
int
dbuf_read_canfail(dmu_buf_impl_t *db)
{
	return (dbuf_read_generic(db, DB_RF_CANFAIL));
}

void
dbuf_read_havestruct(dmu_buf_impl_t *db)
{
	int err;

	ASSERT(RW_LOCK_HELD(&db->db_dnode->dn_struct_rwlock));
	err = dbuf_read_generic(db, (DB_RF_HAVESTRUCT | DB_RF_NOPREFETCH));
	ASSERT(err == 0);
}

static void
dbuf_noread(dmu_buf_impl_t *db)
{
	ASSERT(!refcount_is_zero(&db->db_holds));
	mutex_enter(&db->db_mtx);
	while (db->db_state == DB_READ || db->db_state == DB_FILL)
		cv_wait(&db->db_changed, &db->db_mtx);
	if (db->db_state == DB_UNCACHED) {
		int blksz = (db->db_blkid == DB_BONUS_BLKID) ?
		    DN_MAX_BONUSLEN : db->db.db_size;
		ASSERT(db->db.db_data == NULL);
		dbuf_set_data(db, arc_buf_alloc(db->db_dnode->dn_objset->os_spa,
		    blksz, db));
		db->db_state = DB_FILL;
	} else {
		ASSERT3U(db->db_state, ==, DB_CACHED);
	}
	mutex_exit(&db->db_mtx);
}

/*
 * This is our just-in-time copy function.  It makes a copy of
 * buffers, that have been modified in a previous transaction
 * group, before we modify them in the current active group.
 *
 * This function is used in two places: when we are dirtying a
 * buffer for the first time in a txg, and when we are freeing
 * a range in a dnode that includes this buffer.
 *
 * Note that when we are called from dbuf_free_range() we do
 * not put a hold on the buffer, we just traverse the active
 * dbuf list for the dnode.
 */
static void
dbuf_fix_old_data(dmu_buf_impl_t *db, uint64_t txg)
{
	arc_buf_t **quiescing, **syncing;
	int size = (db->db_blkid == DB_BONUS_BLKID) ?
	    DN_MAX_BONUSLEN : db->db.db_size;

	ASSERT(MUTEX_HELD(&db->db_mtx));
	ASSERT(db->db.db_data != NULL);

	quiescing = &db->db_d.db_data_old[(txg-1)&TXG_MASK];
	syncing = &db->db_d.db_data_old[(txg-2)&TXG_MASK];

	/*
	 * If this buffer is referenced from the current quiescing
	 * transaction group: either make a copy and reset the reference
	 * to point to the copy, or (if there a no active holders) just
	 * null out the current db_data pointer.
	 */
	if (*quiescing == db->db_buf) {
		/*
		 * If the quiescing txg is "dirty", then we better not
		 * be referencing the same buffer from the syncing txg.
		 */
		ASSERT(*syncing != db->db_buf);
		if (refcount_count(&db->db_holds) > db->db_dirtycnt) {
			*quiescing = arc_buf_alloc(
			    db->db_dnode->dn_objset->os_spa, size, db);
			bcopy(db->db.db_data, (*quiescing)->b_data, size);
		} else {
			db->db.db_data = NULL;
			db->db_buf = NULL;
			db->db_state = DB_UNCACHED;
		}
		return;
	}

	/*
	 * If this buffer is referenced from the current syncing
	 * transaction group: either
	 *	1 - make a copy and reset the reference, or
	 *	2 - if there are no holders, just null the current db_data.
	 */
	if (*syncing == db->db_buf) {
		ASSERT3P(*quiescing, ==, NULL);
		ASSERT3U(db->db_dirtycnt, ==, 1);
		if (refcount_count(&db->db_holds) > db->db_dirtycnt) {
			/* we can't copy if we have already started a write */
			ASSERT(*syncing != db->db_data_pending);
			*syncing = arc_buf_alloc(
			    db->db_dnode->dn_objset->os_spa, size, db);
			bcopy(db->db.db_data, (*syncing)->b_data, size);
		} else {
			db->db.db_data = NULL;
			db->db_buf = NULL;
			db->db_state = DB_UNCACHED;
		}
	}
}

void
dbuf_unoverride(dmu_buf_impl_t *db, uint64_t txg)
{
	ASSERT(MUTEX_HELD(&db->db_mtx));
	if (db->db_d.db_overridden_by[txg&TXG_MASK] == IN_DMU_SYNC) {
		db->db_d.db_overridden_by[txg&TXG_MASK] = NULL;
	} else if (db->db_d.db_overridden_by[txg&TXG_MASK] != NULL) {
		/* free this block */
		ASSERT(list_link_active(&db->db_dirty_node[txg&TXG_MASK]) ||
		    db->db_dnode->dn_free_txg == txg);
		if (!BP_IS_HOLE(db->db_d.db_overridden_by[txg&TXG_MASK])) {
			/* XXX can get silent EIO here */
			(void) arc_free(NULL, db->db_dnode->dn_objset->os_spa,
			    txg, db->db_d.db_overridden_by[txg&TXG_MASK],
			    NULL, NULL, ARC_WAIT);
		}
		kmem_free(db->db_d.db_overridden_by[txg&TXG_MASK],
		    sizeof (blkptr_t));
		db->db_d.db_overridden_by[txg&TXG_MASK] = NULL;
		/* release the already-written buffer */
		arc_release(db->db_d.db_data_old[txg&TXG_MASK], db);
	}
}

void
dbuf_free_range(dnode_t *dn, uint64_t blkid, uint64_t nblks, dmu_tx_t *tx)
{
	dmu_buf_impl_t *db, *db_next;
	uint64_t txg = tx->tx_txg;

	dprintf_dnode(dn, "blkid=%llu nblks=%llu\n", blkid, nblks);
	mutex_enter(&dn->dn_dbufs_mtx);
	for (db = list_head(&dn->dn_dbufs); db; db = db_next) {
		db_next = list_next(&dn->dn_dbufs, db);
		if ((db->db_level != 0) || (db->db_blkid == DB_BONUS_BLKID))
			continue;
		dprintf_dbuf(db, "found buf %s\n", "");
		if (db->db_blkid < blkid ||
		    db->db_blkid >= blkid+nblks)
			continue;

		/* found a level 0 buffer in the range */
		if (dbuf_undirty(db, tx))
			continue;

		mutex_enter(&db->db_mtx);
		if (db->db_state == DB_UNCACHED) {
			ASSERT(db->db.db_data == NULL);
			mutex_exit(&db->db_mtx);
			continue;
		}
		if (db->db_state == DB_READ) {
			/* this will be handled in dbuf_read_done() */
			db->db_d.db_freed_in_flight = TRUE;
			mutex_exit(&db->db_mtx);
			continue;
		}
		if (db->db_state == DB_FILL) {
			/* this will be handled in dbuf_rele() */
			db->db_d.db_freed_in_flight = TRUE;
			mutex_exit(&db->db_mtx);
			continue;
		}

		/* make a copy of the data if necessary */
		dbuf_fix_old_data(db, txg);

		if (db->db.db_data) {
			/* fill in with appropriate data */
			arc_release(db->db_buf, db);
			bzero(db->db.db_data, db->db.db_size);
		}
		mutex_exit(&db->db_mtx);
	}
	mutex_exit(&dn->dn_dbufs_mtx);
}

static int
dbuf_new_block(dmu_buf_impl_t *db, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = db->db_objset->os_dsl_dataset;
	uint64_t birth_txg = 0;

	/* Don't count meta-objects */
	if (ds == NULL)
		return (FALSE);

	/*
	 * We don't need any locking to protect db_blkptr:
	 * If it's syncing, then db_dirtied will be set so we'll
	 * ignore db_blkptr.
	 */
	ASSERT(MUTEX_HELD(&db->db_mtx)); /* XXX strictly necessary? */
	/* If we have been dirtied since the last snapshot, its not new */
	if (db->db_dirtied)
		birth_txg = db->db_dirtied;
	else if (db->db_blkptr)
		birth_txg = db->db_blkptr->blk_birth;

	if (birth_txg)
		return (!dsl_dataset_block_freeable(ds, birth_txg, tx));
	else
		return (TRUE);
}

void
dbuf_new_size(dmu_buf_impl_t *db, int size, dmu_tx_t *tx)
{
	arc_buf_t *buf, *obuf;
	int osize = db->db.db_size;

	/* XXX does *this* func really need the lock? */
	ASSERT(RW_WRITE_HELD(&db->db_dnode->dn_struct_rwlock));

	ASSERT3U(osize, <=, size);
	if (osize == size)
		return;

	/*
	 * This call to dbuf_will_dirty() with the dn_struct_rwlock held
	 * is OK, because there can be no other references to the db
	 * when we are changing its size, so no concurrent DB_FILL can
	 * be happening.
	 */
	/* Make a copy of the data if necessary */
	dbuf_will_dirty(db, tx);

	/* create the data buffer for the new block */
	buf = arc_buf_alloc(db->db_dnode->dn_objset->os_spa, size, db);

	/* copy old block data to the new block */
	obuf = db->db_buf;
	bcopy(obuf->b_data, buf->b_data, osize);
	/* zero the remainder */
	bzero((uint8_t *)buf->b_data + osize, size - osize);

	mutex_enter(&db->db_mtx);
	/* ASSERT3U(refcount_count(&db->db_holds), ==, 1); */
	dbuf_set_data(db, buf);
	arc_buf_free(obuf, db);
	db->db.db_size = size;

	/* fix up the dirty info */
	if (db->db_level == 0)
		db->db_d.db_data_old[tx->tx_txg&TXG_MASK] = buf;
	mutex_exit(&db->db_mtx);

	dnode_willuse_space(db->db_dnode, size-osize, tx);
}

void
dbuf_dirty(dmu_buf_impl_t *db, dmu_tx_t *tx)
{
	dnode_t *dn = db->db_dnode;
	objset_impl_t *os = dn->dn_objset;
	int drop_struct_lock = FALSE;
	int txgoff = tx->tx_txg & TXG_MASK;

	ASSERT(tx->tx_txg != 0);
	ASSERT(!refcount_is_zero(&db->db_holds));
	DMU_TX_DIRTY_BUF(tx, db);

	/*
	 * Shouldn't dirty a regular buffer in syncing context.  Private
	 * objects may be dirtied in syncing context, but only if they
	 * were already pre-dirtied in open context.
	 * XXX We may want to prohibit dirtying in syncing context even
	 * if they did pre-dirty.
	 */
	ASSERT(!(dmu_tx_is_syncing(tx) &&
	    !BP_IS_HOLE(&dn->dn_objset->os_rootbp) &&
	    !(dn->dn_object & DMU_PRIVATE_OBJECT) &&
	    dn->dn_objset->os_dsl_dataset != NULL &&
	    !dsl_dir_is_private(
	    dn->dn_objset->os_dsl_dataset->ds_dir)));

	/*
	 * We make this assert for private objects as well, but after we
	 * check if we're already dirty.  They are allowed to re-dirty
	 * in syncing context.
	 */
	ASSERT(dn->dn_object & DMU_PRIVATE_OBJECT ||
	    dn->dn_dirtyctx == DN_UNDIRTIED ||
	    dn->dn_dirtyctx ==
	    (dmu_tx_is_syncing(tx) ? DN_DIRTY_SYNC : DN_DIRTY_OPEN));

	mutex_enter(&db->db_mtx);
	/* XXX make this true for indirects too? */
	ASSERT(db->db_level != 0 || db->db_state == DB_CACHED ||
	    db->db_state == DB_FILL);

	/*
	 * If this buffer is currently part of an "overridden" region,
	 * we now need to remove it from that region.
	 */
	if (db->db_level == 0 && db->db_blkid != DB_BONUS_BLKID &&
	    db->db_d.db_overridden_by[txgoff] != NULL) {
		dbuf_unoverride(db, tx->tx_txg);
	}

	mutex_enter(&dn->dn_mtx);
	/*
	 * Don't set dirtyctx to SYNC if we're just modifying this as we
	 * initialize the objset.
	 */
	if (dn->dn_dirtyctx == DN_UNDIRTIED &&
	    !BP_IS_HOLE(&dn->dn_objset->os_rootbp)) {
		dn->dn_dirtyctx =
		    (dmu_tx_is_syncing(tx) ? DN_DIRTY_SYNC : DN_DIRTY_OPEN);
		ASSERT(dn->dn_dirtyctx_firstset == NULL);
		dn->dn_dirtyctx_firstset = kmem_alloc(1, KM_SLEEP);
	}
	mutex_exit(&dn->dn_mtx);

	/*
	 * If this buffer is already dirty, we're done.
	 */
	if (list_link_active(&db->db_dirty_node[txgoff])) {
		mutex_exit(&db->db_mtx);
		return;
	}

	/*
	 * Only valid if not already dirty.
	 */
	ASSERT(dn->dn_dirtyctx == DN_UNDIRTIED || dn->dn_dirtyctx ==
	    (dmu_tx_is_syncing(tx) ? DN_DIRTY_SYNC : DN_DIRTY_OPEN));

	ASSERT3U(dn->dn_nlevels, >, db->db_level);
	ASSERT((dn->dn_phys->dn_nlevels == 0 && db->db_level == 0) ||
	    dn->dn_phys->dn_nlevels > db->db_level ||
	    dn->dn_next_nlevels[txgoff] > db->db_level ||
	    dn->dn_next_nlevels[(tx->tx_txg-1) & TXG_MASK] > db->db_level ||
	    dn->dn_next_nlevels[(tx->tx_txg-2) & TXG_MASK] > db->db_level);

	/*
	 * We should only be dirtying in syncing context if it's the
	 * mos, a spa os, or we're initializing the os.  However, we are
	 * allowed to dirty in syncing context provided we already
	 * dirtied it in open context.  Hence we must make this
	 * assertion only if we're not already dirty.
	 */
	ASSERT(!dmu_tx_is_syncing(tx) ||
	    os->os_dsl_dataset == NULL ||
	    !dsl_dir_is_private(os->os_dsl_dataset->ds_dir) ||
	    !BP_IS_HOLE(&os->os_rootbp));
	ASSERT(db->db.db_size != 0);

	dprintf_dbuf(db, "size=%llx\n", (u_longlong_t)db->db.db_size);

	if (db->db_level == 0) {
		/*
		 * Release the data buffer from the cache so that we
		 * can modify it without impacting possible other users
		 * of this cached data block.  Note that indirect blocks
		 * and private objects are not released until the syncing
		 * state (since they are only modified then).
		 *
		 * If this buffer is dirty in an old transaction group we need
		 * to make a copy of it so that the changes we make in this
		 * transaction group won't leak out when we sync the older txg.
		 */
		ASSERT(db->db_buf != NULL);
		ASSERT(db->db.db_data != NULL);
		ASSERT(db->db_d.db_data_old[txgoff] == NULL);
		if (!(db->db.db_object & DMU_PRIVATE_OBJECT)) {
			arc_release(db->db_buf, db);
			dbuf_fix_old_data(db, tx->tx_txg);
			ASSERT(db->db_buf != NULL);
		}
		db->db_d.db_data_old[txgoff] = db->db_buf;
	}

	mutex_enter(&dn->dn_mtx);
	/*
	 * We could have been freed_in_flight between the dbuf_noread
	 * and dbuf_dirty.  We win, as though the dbuf_noread() had
	 * happened after the free.
	 */
	if (db->db_level == 0 && db->db_blkid != DB_BONUS_BLKID) {
		dnode_clear_range(dn, db->db_blkid, 1, tx);
		db->db_d.db_freed_in_flight = FALSE;
	}

	db->db_dirtied = tx->tx_txg;
	list_insert_tail(&dn->dn_dirty_dbufs[txgoff], db);
	mutex_exit(&dn->dn_mtx);

	/*
	 * If writting this buffer will consume a new block on disk,
	 * then update the accounting.
	 */
	if (db->db_blkid != DB_BONUS_BLKID) {
		if (!dbuf_new_block(db, tx) && db->db_blkptr) {
			/*
			 * This is only a guess -- if the dbuf is dirty
			 * in a previous txg, we don't know how much
			 * space it will use on disk yet.  We should
			 * really have the struct_rwlock to access
			 * db_blkptr, but since this is just a guess,
			 * it's OK if we get an odd answer.
			 */
			dnode_willuse_space(dn,
			    -BP_GET_ASIZE(db->db_blkptr), tx);
		}
		dnode_willuse_space(dn, db->db.db_size, tx);
	}

	/*
	 * This buffer is now part of this txg
	 */
	dbuf_add_ref(db, (void *)(uintptr_t)tx->tx_txg);
	db->db_dirtycnt += 1;
	ASSERT3U(db->db_dirtycnt, <=, 3);

	mutex_exit(&db->db_mtx);

	if (db->db_blkid == DB_BONUS_BLKID) {
		dnode_setdirty(dn, tx);
		return;
	}

	if (db->db_level == 0)
		dnode_new_blkid(dn, db->db_blkid, tx);

	if (!RW_WRITE_HELD(&dn->dn_struct_rwlock)) {
		rw_enter(&dn->dn_struct_rwlock, RW_READER);
		drop_struct_lock = TRUE;
	}

	if (db->db_level < dn->dn_nlevels-1) {
		int epbs = dn->dn_indblkshift - SPA_BLKPTRSHIFT;
		dmu_buf_impl_t *parent;
		parent = dbuf_hold_level(dn, db->db_level+1,
		    db->db_blkid >> epbs, FTAG);
		if (drop_struct_lock)
			rw_exit(&dn->dn_struct_rwlock);
		dbuf_dirty(parent, tx);
		dbuf_remove_ref(parent, FTAG);
	} else {
		if (drop_struct_lock)
			rw_exit(&dn->dn_struct_rwlock);
	}

	dnode_setdirty(dn, tx);
}

static int
dbuf_undirty(dmu_buf_impl_t *db, dmu_tx_t *tx)
{
	dnode_t *dn = db->db_dnode;
	int txgoff = tx->tx_txg & TXG_MASK;

	ASSERT(tx->tx_txg != 0);

	mutex_enter(&db->db_mtx);

	/*
	 * If this buffer is not dirty, we're done.
	 */
	if (!list_link_active(&db->db_dirty_node[txgoff])) {
		mutex_exit(&db->db_mtx);
		return (0);
	}

	/*
	 * If this buffer is currently held, we cannot undirty
	 * it, since one of the current holders may be in the
	 * middle of an update.  Note that users of dbuf_undirty()
	 * should not place a hold on the dbuf before the call.
	 * XXX - this check assumes we are being called from
	 * dbuf_free_range(), perhaps we should move it there?
	 */
	if (refcount_count(&db->db_holds) > db->db_dirtycnt) {
		mutex_exit(&db->db_mtx);
		mutex_enter(&dn->dn_mtx);
		dnode_clear_range(dn, db->db_blkid, 1, tx);
		mutex_exit(&dn->dn_mtx);
		return (0);
	}

	dprintf_dbuf(db, "size=%llx\n", (u_longlong_t)db->db.db_size);

	dbuf_unoverride(db, tx->tx_txg);

	ASSERT(db->db.db_size != 0);
	if (db->db_level == 0) {
		ASSERT(db->db_buf != NULL);
		ASSERT(db->db_d.db_data_old[txgoff] != NULL);
		if (db->db_d.db_data_old[txgoff] != db->db_buf)
			arc_buf_free(db->db_d.db_data_old[txgoff], db);
		db->db_d.db_data_old[txgoff] = NULL;
	}

	/* XXX would be nice to fix up dn_towrite_space[] */
	/* XXX undo db_dirtied? but how? */
	/* db->db_dirtied = tx->tx_txg; */

	mutex_enter(&dn->dn_mtx);
	list_remove(&dn->dn_dirty_dbufs[txgoff], db);
	mutex_exit(&dn->dn_mtx);

	ASSERT(db->db_dirtycnt > 0);
	db->db_dirtycnt -= 1;

	if (refcount_remove(&db->db_holds,
	    (void *)(uintptr_t)tx->tx_txg) == 0) {
		/* make duf_verify() happy */
		if (db->db.db_data)
			bzero(db->db.db_data, db->db.db_size);

		dbuf_evict(db);
		return (1);
	}

	mutex_exit(&db->db_mtx);
	return (0);
}

#pragma weak dmu_buf_will_dirty = dbuf_will_dirty
void
dbuf_will_dirty(dmu_buf_impl_t *db, dmu_tx_t *tx)
{
	int rf = DB_RF_MUST_SUCCEED;

	ASSERT(tx->tx_txg != 0);
	ASSERT(!refcount_is_zero(&db->db_holds));

	if (RW_WRITE_HELD(&db->db_dnode->dn_struct_rwlock))
		rf |= DB_RF_HAVESTRUCT;
	(void) dbuf_read_generic(db, rf);
	dbuf_dirty(db, tx);
}

#pragma weak dmu_buf_will_fill = dbuf_will_fill
void
dbuf_will_fill(dmu_buf_impl_t *db, dmu_tx_t *tx)
{
	ASSERT(tx->tx_txg != 0);
	ASSERT(db->db_level == 0);
	ASSERT(!refcount_is_zero(&db->db_holds));

	ASSERT(!(db->db.db_object & DMU_PRIVATE_OBJECT) ||
	    dmu_tx_private_ok(tx));

	dbuf_noread(db);
	dbuf_dirty(db, tx);
}

#pragma weak dmu_buf_fill_done = dbuf_fill_done
/* ARGSUSED */
void
dbuf_fill_done(dmu_buf_impl_t *db, dmu_tx_t *tx)
{
	mutex_enter(&db->db_mtx);
	DBUF_VERIFY(db);

	if (db->db_state == DB_FILL) {
		if (db->db_level == 0 && db->db_d.db_freed_in_flight) {
			/* we were freed while filling */
			/* XXX dbuf_undirty? */
			bzero(db->db.db_data, db->db.db_size);
			db->db_d.db_freed_in_flight = FALSE;
		}
		db->db_state = DB_CACHED;
		cv_broadcast(&db->db_changed);
	}
	mutex_exit(&db->db_mtx);
}


static void
dbuf_clear(dmu_buf_impl_t *db)
{
	dnode_t *dn = db->db_dnode;

	ASSERT(MUTEX_HELD(&dn->dn_dbufs_mtx));
	ASSERT(MUTEX_HELD(&db->db_mtx));
	ASSERT(refcount_is_zero(&db->db_holds));

	if (db->db_state == DB_CACHED) {
		ASSERT(db->db_buf != NULL);
		arc_buf_free(db->db_buf, db);
		db->db.db_data = NULL;
		db->db_buf = NULL;
		db->db_state = DB_UNCACHED;
	}

	ASSERT3U(db->db_state, ==, DB_UNCACHED);
	ASSERT(db->db_buf == NULL);
	ASSERT(db->db_data_pending == NULL);

	mutex_exit(&db->db_mtx);

	/*
	 * If this dbuf is referened from an indirect dbuf,
	 * decrement the ref count on the indirect dbuf.
	 */
	if (db->db_parent && db->db_parent != dn->dn_dbuf)
		dbuf_remove_ref(db->db_parent, db);

	/* remove from dn_dbufs */
	list_remove(&dn->dn_dbufs, db);

	dnode_rele(dn, db);

	dbuf_hash_remove(db);

	db->db_dnode = NULL;
	db->db_parent = NULL;
	db->db_blkptr = NULL;
}

static int
dbuf_findbp(dnode_t *dn, int level, uint64_t blkid, int fail_sparse,
    dmu_buf_impl_t **parentp, blkptr_t **bpp)
{
	int nlevels, epbs;

	if (dn->dn_phys->dn_nlevels == 0)
		nlevels = 1;
	else
		nlevels = dn->dn_phys->dn_nlevels;

	epbs = dn->dn_indblkshift - SPA_BLKPTRSHIFT;

	ASSERT3U(level * epbs, <, 64);
	ASSERT(RW_LOCK_HELD(&dn->dn_struct_rwlock));
	if (blkid == DB_BONUS_BLKID) {
		/* this is the bonus buffer */
		*parentp = NULL;
		*bpp = NULL;
		return (0);
	} else if (level >= nlevels ||
	    (blkid > (dn->dn_phys->dn_maxblkid >> (level * epbs)))) {
		/* the buffer has no parent yet */
		*parentp = NULL;
		*bpp = NULL;
		return (ENOENT);
	} else if (level < nlevels-1) {
		/* this block is referenced from an indirect block */
		int err = dbuf_hold_impl(dn, level+1,
		    blkid >> epbs, fail_sparse, NULL, parentp);
		if (err)
			return (err);
		dbuf_read_havestruct(*parentp);
		*bpp = ((blkptr_t *)(*parentp)->db.db_data) +
		    (blkid & ((1ULL << epbs) - 1));
		return (0);
	} else {
		/* the block is referenced from the dnode */
		ASSERT3U(level, ==, nlevels-1);
		ASSERT(dn->dn_phys->dn_nblkptr == 0 ||
		    blkid < dn->dn_phys->dn_nblkptr);
		*parentp = dn->dn_dbuf;
		*bpp = &dn->dn_phys->dn_blkptr[blkid];
		return (0);
	}
}

static dmu_buf_impl_t *
dbuf_create(dnode_t *dn, uint8_t level, uint64_t blkid,
    dmu_buf_impl_t *parent, blkptr_t *blkptr)
{
	objset_impl_t *os = dn->dn_objset;
	dmu_buf_impl_t *db, *odb;

	ASSERT(RW_LOCK_HELD(&dn->dn_struct_rwlock));
	ASSERT(dn->dn_type != DMU_OT_NONE);

	db = kmem_cache_alloc(dbuf_cache, KM_SLEEP);

	db->db_objset = os;
	db->db.db_object = dn->dn_object;
	db->db_level = level;
	db->db_blkid = blkid;
	db->db_state = DB_UNCACHED;

	if (db->db_blkid == DB_BONUS_BLKID) {
		db->db.db_size = dn->dn_bonuslen;
		db->db.db_offset = DB_BONUS_BLKID;
	} else {
		int blocksize =
		    db->db_level ? 1<<dn->dn_indblkshift :  dn->dn_datablksz;
		db->db.db_size = blocksize;
		db->db.db_offset = db->db_blkid * blocksize;
	}

	db->db_dirtied = 0;
	db->db_dirtycnt = 0;

	bzero(&db->db_d, sizeof (db->db_d));

	/*
	 * Hold the dn_dbufs_mtx while we get the new dbuf
	 * in the hash table *and* added to the dbufs list.
	 * This prevents a possible deadlock with someone
	 * trying to look up this dbuf before its added to the
	 * dn_dbufs list.
	 */
	mutex_enter(&dn->dn_dbufs_mtx);
	if ((odb = dbuf_hash_insert(db)) != NULL) {
		/* someone else inserted it first */
		kmem_cache_free(dbuf_cache, db);
		mutex_exit(&dn->dn_dbufs_mtx);
		return (odb);
	}
	list_insert_head(&dn->dn_dbufs, db);
	mutex_exit(&dn->dn_dbufs_mtx);

	if (parent && parent != dn->dn_dbuf)
		dbuf_add_ref(parent, db);

	(void) refcount_add(&dn->dn_holds, db);

	db->db_dnode = dn;
	db->db_parent = parent;
	db->db_blkptr = blkptr;

	dprintf_dbuf(db, "db=%p\n", db);

	return (db);
}

static int
dbuf_evictable(dmu_buf_impl_t *db)
{
	int i;

	ASSERT(MUTEX_HELD(&db->db_mtx));
	DBUF_VERIFY(db);

	if (db->db_state != DB_UNCACHED && db->db_state != DB_CACHED)
		return (FALSE);

	if (!refcount_is_zero(&db->db_holds))
		return (FALSE);

#ifdef ZFS_DEBUG
	for (i = 0; i < TXG_SIZE; i++) {
		ASSERT(!list_link_active(&db->db_dirty_node[i]));
		ASSERT(db->db_level != 0 || db->db_d.db_data_old[i] == NULL);
	}
#endif

	/*
	 * Now we know we want to free it.
	 * This call must be done last, since it has side effects -
	 * calling the db_evict_func().
	 */
	dbuf_evict_user(db);
	return (TRUE);
}

static void
dbuf_destroy(dmu_buf_impl_t *db)
{
	ASSERT(refcount_is_zero(&db->db_holds));

	ASSERT(db->db.db_data == NULL);
	ASSERT(db->db_dnode == NULL);
	ASSERT(db->db_parent == NULL);
	ASSERT(db->db_hash_next == NULL);
	ASSERT(db->db_blkptr == NULL);
	ASSERT(db->db_data_pending == NULL);

	kmem_cache_free(dbuf_cache, db);
}

void
dbuf_prefetch(dnode_t *dn, uint64_t blkid)
{
	dmu_buf_impl_t *db, *parent = NULL;
	blkptr_t *bp = NULL;

	ASSERT(blkid != DB_BONUS_BLKID);
	ASSERT(RW_LOCK_HELD(&dn->dn_struct_rwlock));

	if (dnode_block_freed(dn, blkid))
		return;

	/* dbuf_find() returns with db_mtx held */
	if (db = dbuf_find(dn, 0, blkid)) {
		/*
		 * This dbuf is already in the cache.  We assume that
		 * it is already CACHED, or else about to be either
		 * read or filled.
		 */
		mutex_exit(&db->db_mtx);
		return;
	}

	if (dbuf_findbp(dn, 0, blkid, TRUE, &parent, &bp) == 0) {
		if (bp && !BP_IS_HOLE(bp)) {
			(void) arc_read(NULL, dn->dn_objset->os_spa, bp,
			    dmu_ot[dn->dn_type].ot_byteswap,
			    NULL, NULL, ZIO_PRIORITY_ASYNC_READ,
			    ZIO_FLAG_CANFAIL | ZIO_FLAG_SPECULATIVE,
			    (ARC_NOWAIT | ARC_PREFETCH));
		}
		if (parent && parent != dn->dn_dbuf)
			dbuf_rele(parent);
	}
}

/*
 * Returns with db_holds incremented, and db_mtx not held.
 * Note: dn_struct_rwlock must be held.
 */
int
dbuf_hold_impl(dnode_t *dn, uint8_t level, uint64_t blkid, int fail_sparse,
    void *tag, dmu_buf_impl_t **dbp)
{
	dmu_buf_impl_t *db, *parent = NULL;

	ASSERT(RW_LOCK_HELD(&dn->dn_struct_rwlock));
	ASSERT3U(dn->dn_nlevels, >, level);

	*dbp = NULL;

	/* dbuf_find() returns with db_mtx held */
	db = dbuf_find(dn, level, blkid);

	if (db == NULL) {
		blkptr_t *bp = NULL;
		int err;

		err = dbuf_findbp(dn, level, blkid, fail_sparse, &parent, &bp);
		if (fail_sparse) {
			if (err == 0 && bp && BP_IS_HOLE(bp))
				err = ENOENT;
			if (err) {
				if (parent && parent != dn->dn_dbuf)
					dbuf_rele(parent);
				return (err);
			}
		}
		db = dbuf_create(dn, level, blkid, parent, bp);
	}

	/*
	 * If this buffer is currently syncing out, and we are
	 * are still referencing it from db_data, we need to make
	 * a copy of it in case we decide we want to dirty it
	 * again in this txg.
	 */
	if (db->db_level == 0 && db->db_state == DB_CACHED &&
	    !(dn->dn_object & DMU_PRIVATE_OBJECT) &&
	    db->db_data_pending == db->db_buf) {
		int size = (db->db_blkid == DB_BONUS_BLKID) ?
		    DN_MAX_BONUSLEN : db->db.db_size;

		dbuf_set_data(db, arc_buf_alloc(db->db_dnode->dn_objset->os_spa,
		    size, db));
		bcopy(db->db_data_pending->b_data, db->db.db_data,
		    db->db.db_size);
	}

	dbuf_add_ref(db, tag);
	dbuf_update_data(db);
	DBUF_VERIFY(db);
	mutex_exit(&db->db_mtx);

	/* NOTE: we can't rele the parent until after we drop the db_mtx */
	if (parent && parent != dn->dn_dbuf)
		dbuf_rele(parent);

	ASSERT3P(db->db_dnode, ==, dn);
	ASSERT3U(db->db_blkid, ==, blkid);
	ASSERT3U(db->db_level, ==, level);
	*dbp = db;

	return (0);
}

dmu_buf_impl_t *
dbuf_hold(dnode_t *dn, uint64_t blkid)
{
	dmu_buf_impl_t *db;
	(void) dbuf_hold_impl(dn, 0, blkid, FALSE, NULL, &db);
	return (db);
}

dmu_buf_impl_t *
dbuf_hold_level(dnode_t *dn, int level, uint64_t blkid, void *tag)
{
	dmu_buf_impl_t *db;
	(void) dbuf_hold_impl(dn, level, blkid, FALSE, tag, &db);
	return (db);
}

dmu_buf_impl_t *
dbuf_hold_bonus(dnode_t *dn, void *tag)
{
	dmu_buf_impl_t *db;
	rw_enter(&dn->dn_struct_rwlock, RW_READER);
	(void) dbuf_hold_impl(dn, 0, DB_BONUS_BLKID, FALSE, tag, &db);
	rw_exit(&dn->dn_struct_rwlock);
	return (db);
}

void
dbuf_add_ref(dmu_buf_impl_t *db, void *tag)
{
	(void) refcount_add(&db->db_holds, tag);
	/* dprintf_dbuf(db, "adding ref %p; holds up to %lld\n", tag, holds); */
}

void
dbuf_remove_ref(dmu_buf_impl_t *db, void *tag)
{
	int64_t holds;
	dnode_t *dn = db->db_dnode;
	int need_mutex;

	ASSERT(dn != NULL);
	need_mutex = !MUTEX_HELD(&dn->dn_dbufs_mtx);

	if (need_mutex) {
		dnode_add_ref(dn, FTAG);
		mutex_enter(&dn->dn_dbufs_mtx);
	}

	mutex_enter(&db->db_mtx);
	DBUF_VERIFY(db);

	holds = refcount_remove(&db->db_holds, tag);

	if (holds == 0) {
		ASSERT3U(db->db_state, !=, DB_FILL);
		if (db->db_level == 0 &&
		    db->db_d.db_user_data_ptr_ptr != NULL)
			*db->db_d.db_user_data_ptr_ptr = NULL;
		dbuf_evict(db);
	} else {
		if (holds == db->db_dirtycnt &&
		    db->db_level == 0 && db->db_d.db_immediate_evict)
			dbuf_evict_user(db);
		mutex_exit(&db->db_mtx);
	}

	if (need_mutex) {
		mutex_exit(&dn->dn_dbufs_mtx);
		dnode_rele(dn, FTAG);
	}
}

void
dbuf_rele(dmu_buf_impl_t *db)
{
	dbuf_remove_ref(db, NULL);
}

#pragma weak dmu_buf_refcount = dbuf_refcount
uint64_t
dbuf_refcount(dmu_buf_impl_t *db)
{
	return (refcount_count(&db->db_holds));
}

void *
dmu_buf_set_user(dmu_buf_t *db_fake, void *user_ptr, void *user_data_ptr_ptr,
    dmu_buf_evict_func_t *evict_func)
{
	return (dmu_buf_update_user(db_fake, NULL, user_ptr,
	    user_data_ptr_ptr, evict_func));
}

void *
dmu_buf_set_user_ie(dmu_buf_t *db_fake, void *user_ptr, void *user_data_ptr_ptr,
    dmu_buf_evict_func_t *evict_func)
{
	dmu_buf_impl_t *db = (dmu_buf_impl_t *)db_fake;

	db->db_d.db_immediate_evict = TRUE;
	return (dmu_buf_update_user(db_fake, NULL, user_ptr,
	    user_data_ptr_ptr, evict_func));
}

void *
dmu_buf_update_user(dmu_buf_t *db_fake, void *old_user_ptr, void *user_ptr,
    void *user_data_ptr_ptr, dmu_buf_evict_func_t *evict_func)
{
	dmu_buf_impl_t *db = (dmu_buf_impl_t *)db_fake;
	ASSERT(db->db_level == 0);

	ASSERT((user_ptr == NULL) == (evict_func == NULL));

	mutex_enter(&db->db_mtx);

	if (db->db_d.db_user_ptr == old_user_ptr) {
		db->db_d.db_user_ptr = user_ptr;
		db->db_d.db_user_data_ptr_ptr = user_data_ptr_ptr;
		db->db_d.db_evict_func = evict_func;

		dbuf_update_data(db);
	} else {
		old_user_ptr = db->db_d.db_user_ptr;
	}

	mutex_exit(&db->db_mtx);
	return (old_user_ptr);
}

void *
dmu_buf_get_user(dmu_buf_t *db_fake)
{
	dmu_buf_impl_t *db = (dmu_buf_impl_t *)db_fake;
	ASSERT(!refcount_is_zero(&db->db_holds));

	return (db->db_d.db_user_ptr);
}

void
dbuf_sync(dmu_buf_impl_t *db, zio_t *zio, dmu_tx_t *tx)
{
	arc_buf_t **data;
	uint64_t txg = tx->tx_txg;
	dnode_t *dn = db->db_dnode;
	objset_impl_t *os = dn->dn_objset;
	int blksz;

	ASSERT(dmu_tx_is_syncing(tx));

	dprintf_dbuf_bp(db, db->db_blkptr, "blkptr=%p", db->db_blkptr);

	mutex_enter(&db->db_mtx);
	/*
	 * To be synced, we must be dirtied.  But we
	 * might have been freed after the dirty.
	 */
	if (db->db_state == DB_UNCACHED) {
		/* This buffer has been freed since it was dirtied */
		ASSERT(db->db.db_data == NULL);
	} else if (db->db_state == DB_FILL) {
		/* This buffer was freed and is now being re-filled */
		ASSERT(db->db.db_data != db->db_d.db_data_old[txg&TXG_MASK]);
	} else {
		ASSERT3U(db->db_state, ==, DB_CACHED);
	}
	DBUF_VERIFY(db);

	/*
	 * Don't need a lock on db_dirty (dn_mtx), because it can't
	 * be modified yet.
	 */

	if (db->db_level == 0) {
		data = &db->db_d.db_data_old[txg&TXG_MASK];
		blksz = arc_buf_size(*data);
		/*
		 * If this buffer is currently "in use" (i.e., there are
		 * active holds and db_data still references it), then make
		 * a copy before we start the write so that any modifications
		 * from the open txg will not leak into this write.
		 *
		 * NOTE: this copy does not need to be made for objects only
		 * modified in the syncing context (e.g. DNONE_DNODE blocks)
		 * or if there is no actual write involved (bonus blocks).
		 */
		if (!(dn->dn_object & DMU_PRIVATE_OBJECT) &&
		    db->db_d.db_overridden_by[txg&TXG_MASK] == NULL &&
		    db->db_blkid != DB_BONUS_BLKID) {
			if (refcount_count(&db->db_holds) > 1 &&
			    *data == db->db_buf) {
				*data = arc_buf_alloc(
				    db->db_dnode->dn_objset->os_spa, blksz, db);
				bcopy(db->db.db_data, (*data)->b_data, blksz);
			}
			db->db_data_pending = *data;
		} else if (dn->dn_object & DMU_PRIVATE_OBJECT) {
			/*
			 * Private object buffers are released here rather
			 * than in dbuf_dirty() since they are only modified
			 * in the syncing context and we don't want the
			 * overhead of making multiple copies of the data.
			 */
			arc_release(db->db_buf, db);
		}
	} else {
		data = &db->db_buf;
		if (*data == NULL) {
			/*
			 * This can happen if we dirty and then free
			 * the level-0 data blocks in the same txg. So
			 * this indirect remains unchanged.
			 */
			if (db->db_dirtied == txg)
				db->db_dirtied = 0;
			ASSERT(db->db_dirtycnt > 0);
			db->db_dirtycnt -= 1;
			mutex_exit(&db->db_mtx);
			dbuf_remove_ref(db, (void *)(uintptr_t)txg);
			return;
		}
		blksz = db->db.db_size;
		ASSERT3U(blksz, ==, 1<<dn->dn_phys->dn_indblkshift);
	}

	ASSERT(*data != NULL);

	if (db->db_blkid == DB_BONUS_BLKID) {
		/*
		 * Simply copy the bonus data into the dnode.  It will
		 * be written out when the dnode is synced (and it will
		 * be synced, since it must have been dirty for dbuf_sync
		 * to be called).  The bonus data will be byte swapped
		 * in dnode_byteswap.
		 */
		/*
		 * Use dn_phys->dn_bonuslen since db.db_size is the length
		 * of the bonus buffer in the open transaction rather than
		 * the syncing transaction.
		 */
		ASSERT3U(db->db_level, ==, 0);
		ASSERT3U(dn->dn_phys->dn_bonuslen, <=, blksz);
		bcopy((*data)->b_data, DN_BONUS(dn->dn_phys),
		    dn->dn_phys->dn_bonuslen);
		if (*data != db->db_buf)
			arc_buf_free(*data, db);
		db->db_d.db_data_old[txg&TXG_MASK] = NULL;
		db->db_data_pending = NULL;
		if (db->db_dirtied == txg)
			db->db_dirtied = 0;
		ASSERT(db->db_dirtycnt > 0);
		db->db_dirtycnt -= 1;
		mutex_exit(&db->db_mtx);
		dbuf_remove_ref(db, (void *)(uintptr_t)txg);
		return;
	} else if (db->db_level > 0 && !arc_released(db->db_buf)) {
		/*
		 * This indirect buffer was marked dirty, but
		 * never modified (if it had been modified, then
		 * we would have released the buffer).  There is
		 * no reason to write anything.
		 */
		db->db_data_pending = NULL;
		if (db->db_dirtied == txg)
			db->db_dirtied = 0;
		ASSERT(db->db_dirtycnt > 0);
		db->db_dirtycnt -= 1;
		mutex_exit(&db->db_mtx);
		dbuf_remove_ref(db, (void *)(uintptr_t)txg);
		return;
	} else if (db->db_blkptr == NULL &&
	    db->db_level == dn->dn_phys->dn_nlevels-1 &&
	    db->db_blkid < dn->dn_phys->dn_nblkptr) {
		/*
		 * This buffer was allocated at a time when there was
		 * no available blkptrs from the dnode, or it was
		 * inappropriate to hook it in (i.e., nlevels mis-match).
		 */
		ASSERT(db->db_blkptr == NULL);
		ASSERT(db->db_parent == NULL);
		db->db_parent = dn->dn_dbuf;
		db->db_blkptr = &dn->dn_phys->dn_blkptr[db->db_blkid];
		DBUF_VERIFY(db);
		mutex_exit(&db->db_mtx);
	} else if (db->db_blkptr == NULL) {
		dmu_buf_impl_t *parent = db->db_parent;
		int epbs = dn->dn_phys->dn_indblkshift - SPA_BLKPTRSHIFT;

		mutex_exit(&db->db_mtx);
		ASSERT(dn->dn_phys->dn_nlevels > 1);
		if (parent == NULL) {
			rw_enter(&dn->dn_struct_rwlock, RW_READER);
			(void) dbuf_hold_impl(dn, db->db_level+1,
			    db->db_blkid >> epbs, FALSE, NULL, &parent);
			rw_exit(&dn->dn_struct_rwlock);
			dbuf_add_ref(parent, db);
			db->db_parent = parent;
			dbuf_rele(parent);
		}
		dbuf_read(parent);
	} else {
		mutex_exit(&db->db_mtx);
	}

	ASSERT(IS_DNODE_DNODE(dn->dn_object) || db->db_parent != NULL);

	if (db->db_parent != dn->dn_dbuf) {
		dmu_buf_impl_t *parent = db->db_parent;
		int epbs = dn->dn_phys->dn_indblkshift - SPA_BLKPTRSHIFT;

		mutex_enter(&db->db_mtx);
		ASSERT(db->db_level == parent->db_level-1);
		ASSERT(list_link_active(&parent->db_dirty_node[txg&TXG_MASK]));
		/*
		 * We may have read this block after we dirtied it,
		 * so never released it from the cache.
		 */
		arc_release(parent->db_buf, parent);

		db->db_blkptr = (blkptr_t *)parent->db.db_data +
		    (db->db_blkid & ((1ULL << epbs) - 1));
		DBUF_VERIFY(db);
		mutex_exit(&db->db_mtx);
	}
	ASSERT(db->db_parent == NULL || arc_released(db->db_parent->db_buf));

#ifdef ZFS_DEBUG
	if (db->db_parent == dn->dn_dbuf) {
		/*
		 * We don't need to dnode_setdirty(dn) because if we got
		 * here then the parent is already dirty.
		 */
		ASSERT(db->db_level == dn->dn_phys->dn_nlevels-1);
		ASSERT3P(db->db_blkptr, ==,
		    &dn->dn_phys->dn_blkptr[db->db_blkid]);
	}
#endif
	if (db->db_level == 0 &&
	    db->db_d.db_overridden_by[txg&TXG_MASK] != NULL) {
		arc_buf_t **old = &db->db_d.db_data_old[txg&TXG_MASK];
		blkptr_t **bpp = &db->db_d.db_overridden_by[txg&TXG_MASK];
		int old_size = BP_GET_ASIZE(db->db_blkptr);
		int new_size = BP_GET_ASIZE(*bpp);

		ASSERT(db->db_blkid != DB_BONUS_BLKID);

		dnode_diduse_space(dn, new_size-old_size);
		mutex_enter(&dn->dn_mtx);
		if (db->db_blkid > dn->dn_phys->dn_maxblkid)
			dn->dn_phys->dn_maxblkid = db->db_blkid;
		mutex_exit(&dn->dn_mtx);

		dsl_dataset_block_born(os->os_dsl_dataset, *bpp, tx);
		if (!BP_IS_HOLE(db->db_blkptr))
			dsl_dataset_block_kill(os->os_dsl_dataset,
			    db->db_blkptr, os->os_synctx);

		mutex_enter(&db->db_mtx);
		*db->db_blkptr = **bpp;
		kmem_free(*bpp, sizeof (blkptr_t));
		*bpp = NULL;

		if (*old != db->db_buf)
			arc_buf_free(*old, db);
		*old = NULL;
		db->db_data_pending = NULL;

		cv_broadcast(&db->db_changed);

		ASSERT(db->db_dirtycnt > 0);
		db->db_dirtycnt -= 1;
		mutex_exit(&db->db_mtx);
		dbuf_remove_ref(db, (void *)(uintptr_t)txg);
	} else {
		int checksum, compress;

		if (db->db_level > 0) {
			/*
			 * XXX -- we should design a compression algorithm
			 * that specializes in arrays of bps.
			 */
			checksum = ZIO_CHECKSUM_FLETCHER_4;
			compress = ZIO_COMPRESS_LZJB;
		} else {
			/*
			 * Allow dnode settings to override objset settings,
			 * except for metadata checksums.
			 */
			if (dmu_ot[dn->dn_type].ot_metadata) {
				checksum = os->os_md_checksum;
				compress = zio_compress_select(dn->dn_compress,
				    os->os_md_compress);
			} else {
				checksum = zio_checksum_select(dn->dn_checksum,
				    os->os_checksum);
				compress = zio_compress_select(dn->dn_compress,
				    os->os_compress);
			}
		}
#ifdef ZFS_DEBUG
		if (db->db_parent) {
			ASSERT(list_link_active(
			    &db->db_parent->db_dirty_node[txg&TXG_MASK]));
			ASSERT(db->db_parent == dn->dn_dbuf ||
			    db->db_parent->db_level > 0);
			if (dn->dn_object & DMU_PRIVATE_OBJECT ||
			    db->db_level > 0)
				ASSERT(*data == db->db_buf);
		}
#endif
		ASSERT3U(db->db_blkptr->blk_birth, <=, tx->tx_txg);
		(void) arc_write(zio, os->os_spa, checksum, compress, txg,
		    db->db_blkptr, *data, dbuf_write_done, db,
		    ZIO_PRIORITY_ASYNC_WRITE, ZIO_FLAG_MUSTSUCCEED, ARC_NOWAIT);
		/*
		 * We can't access db after arc_write, since it could finish
		 * and be freed, and we have no locks on it.
		 */
	}
}

struct dbuf_arg {
	objset_impl_t *os;
	blkptr_t bp;
};

static void
dbuf_do_born(void *arg)
{
	struct dbuf_arg *da = arg;
	dsl_dataset_block_born(da->os->os_dsl_dataset,
	    &da->bp, da->os->os_synctx);
	kmem_free(da, sizeof (struct dbuf_arg));
}

static void
dbuf_do_kill(void *arg)
{
	struct dbuf_arg *da = arg;
	dsl_dataset_block_kill(da->os->os_dsl_dataset,
	    &da->bp, da->os->os_synctx);
	kmem_free(da, sizeof (struct dbuf_arg));
}

/* ARGSUSED */
static void
dbuf_write_done(zio_t *zio, arc_buf_t *buf, void *vdb)
{
	dmu_buf_impl_t *db = vdb;
	dnode_t *dn = db->db_dnode;
	objset_impl_t *os = dn->dn_objset;
	uint64_t txg = zio->io_txg;
	uint64_t fill = 0;
	int i;
	int old_size, new_size;

	ASSERT3U(zio->io_error, ==, 0);

	dprintf_dbuf_bp(db, &zio->io_bp_orig, "bp_orig: %s", "");

	old_size = BP_GET_ASIZE(&zio->io_bp_orig);
	new_size = BP_GET_ASIZE(zio->io_bp);

	dnode_diduse_space(dn, new_size-old_size);

	mutex_enter(&db->db_mtx);

	if (db->db_dirtied == txg)
		db->db_dirtied = 0;

	if (db->db_level == 0) {
		arc_buf_t **old = &db->db_d.db_data_old[txg&TXG_MASK];

		ASSERT(db->db_blkid != DB_BONUS_BLKID);

		if (*old != db->db_buf)
			arc_buf_free(*old, db);
		*old = NULL;
		db->db_data_pending = NULL;

		mutex_enter(&dn->dn_mtx);
		if (db->db_blkid > dn->dn_phys->dn_maxblkid &&
		    !BP_IS_HOLE(db->db_blkptr))
			dn->dn_phys->dn_maxblkid = db->db_blkid;
		mutex_exit(&dn->dn_mtx);

		if (dn->dn_type == DMU_OT_DNODE) {
			dnode_phys_t *dnp = db->db.db_data;
			for (i = db->db.db_size >> DNODE_SHIFT; i > 0;
			    i--, dnp++) {
				if (dnp->dn_type != DMU_OT_NONE)
					fill++;
			}
		} else {
			if (!BP_IS_HOLE(db->db_blkptr))
				fill = 1;
		}
	} else {
		blkptr_t *bp = db->db.db_data;
		ASSERT3U(db->db.db_size, ==, 1<<dn->dn_phys->dn_indblkshift);
		if (!BP_IS_HOLE(db->db_blkptr)) {
			ASSERT3U(BP_GET_LSIZE(zio->io_bp), ==, db->db.db_size);
			ASSERT3U(BP_GET_LSIZE(db->db_blkptr), ==,
			    db->db.db_size);
		}
		for (i = db->db.db_size >> SPA_BLKPTRSHIFT; i > 0; i--, bp++) {
			if (BP_IS_HOLE(bp))
				continue;
			ASSERT3U(BP_GET_LSIZE(bp), ==,
			    db->db_level == 1 ? dn->dn_datablksz :
			    (1<<dn->dn_phys->dn_indblkshift));
			fill += bp->blk_fill;
		}
	}

	if (!BP_IS_HOLE(db->db_blkptr)) {
		db->db_blkptr->blk_fill = fill;
		BP_SET_TYPE(db->db_blkptr, dn->dn_type);
		BP_SET_LEVEL(db->db_blkptr, db->db_level);
	} else {
		ASSERT3U(fill, ==, 0);
		ASSERT3U(db->db_blkptr->blk_fill, ==, 0);
	}

	dprintf_dbuf_bp(db, db->db_blkptr,
	    "wrote %llu bytes to blkptr:", zio->io_size);

	ASSERT(db->db_parent == NULL ||
	    list_link_active(&db->db_parent->db_dirty_node[txg&TXG_MASK]));
	cv_broadcast(&db->db_changed);
	ASSERT(db->db_dirtycnt > 0);
	db->db_dirtycnt -= 1;
	mutex_exit(&db->db_mtx);

	/* We must do this after we've set the bp's type and level */
	if (!DVA_EQUAL(BP_IDENTITY(zio->io_bp),
	    BP_IDENTITY(&zio->io_bp_orig))) {
		struct dbuf_arg *da;
		da = kmem_alloc(sizeof (struct dbuf_arg), KM_SLEEP);
		da->os = os;
		da->bp = *zio->io_bp;
		(void) taskq_dispatch(dbuf_tq, dbuf_do_born, da, 0);
		if (!BP_IS_HOLE(&zio->io_bp_orig)) {
			da = kmem_alloc(sizeof (struct dbuf_arg), KM_SLEEP);
			da->os = os;
			da->bp = zio->io_bp_orig;
			(void) taskq_dispatch(dbuf_tq, dbuf_do_kill, da, 0);
		}
	}

	dbuf_remove_ref(db, (void *)(uintptr_t)txg);
}
