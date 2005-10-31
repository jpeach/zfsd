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

#ifndef	_SYS_ZAP_LEAF_H
#define	_SYS_ZAP_LEAF_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct zap;

#define	ZAP_LEAF_MAGIC 0x2AB1EAF

/* chunk size = 24 bytes */

#define	ZAP_LEAF_NUMCHUNKS 5118
#define	ZAP_LEAF_ARRAY_BYTES 21
#define	ZAP_LEAF_HASH_SHIFT 12
#define	ZAP_LEAF_HASH_NUMENTRIES (1 << ZAP_LEAF_HASH_SHIFT)
#define	ZAP_LLA_DATA_BYTES ((1 << ZAP_BLOCK_SHIFT) - 16)

typedef enum zap_entry_type {
	ZAP_LEAF_FREE = 253,
	ZAP_LEAF_ENTRY = 252,
	ZAP_LEAF_ARRAY = 251,
	ZAP_LEAF_TYPE_MAX = 250
} zap_entry_type_t;

/*
 * TAKE NOTE:
 * If zap_leaf_phys_t is modified, zap_leaf_byteswap() must be modified.
 */
typedef struct zap_leaf_phys {
	struct zap_leaf_header {
		uint64_t lhr_block_type;	/* ZBT_LEAF */
		uint64_t lhr_next;		/* next block in leaf chain */
		uint64_t lhr_prefix;
		uint32_t lhr_magic;		/* ZAP_LEAF_MAGIC */
		uint16_t lhr_nfree;		/* number free chunks */
		uint16_t lhr_nentries;		/* number of entries */
		uint16_t lhr_prefix_len;

#define	lh_block_type 	l_phys->l_hdr.lhr_block_type
#define	lh_magic 	l_phys->l_hdr.lhr_magic
#define	lh_next 	l_phys->l_hdr.lhr_next
#define	lh_prefix 	l_phys->l_hdr.lhr_prefix
#define	lh_nfree 	l_phys->l_hdr.lhr_nfree
#define	lh_prefix_len 	l_phys->l_hdr.lhr_prefix_len
#define	lh_nentries 	l_phys->l_hdr.lhr_nentries

/* above is accessable to zap, below is zap_leaf private */

		uint16_t lh_freelist;		/* chunk head of free list */
		uint8_t lh_pad2[12];
	} l_hdr; /* 2 24-byte chunks */

	uint16_t l_hash[ZAP_LEAF_HASH_NUMENTRIES];
	/* 170 24-byte chunks plus 16 bytes leftover space */

	union zap_leaf_chunk {
		struct zap_leaf_entry {
			uint8_t le_type; 	/* always ZAP_LEAF_ENTRY */
			uint8_t le_int_size;	/* size of ints */
			uint16_t le_next;	/* next entry in hash chain */
			uint16_t le_name_chunk;	/* first chunk of the name */
			uint16_t le_name_length; /* bytes in name, incl null */
			uint16_t le_value_chunk; /* first chunk of the value */
			uint16_t le_value_length; /* value length in ints */
			uint32_t le_cd;		/* collision differentiator */
			uint64_t le_hash;	/* hash value of the name */
		} l_entry;
		struct zap_leaf_array {
			uint8_t la_type;
			uint8_t la_array[ZAP_LEAF_ARRAY_BYTES];
			uint16_t la_next;	/* next blk or CHAIN_END */
		} l_array;
		struct zap_leaf_free {
			uint8_t lf_type;	/* always ZAP_LEAF_FREE */
			uint8_t lf_pad[ZAP_LEAF_ARRAY_BYTES];
			uint16_t lf_next;  /* next in free list, or CHAIN_END */
		} l_free;
	} l_chunk[ZAP_LEAF_NUMCHUNKS];
} zap_leaf_phys_t;

typedef struct zap_leaf {
	krwlock_t l_rwlock; 		/* only used on head of chain */
	uint64_t l_blkid;		/* 1<<ZAP_BLOCK_SHIFT byte block off */
	struct zap_leaf *l_next;	/* next in chain */
	dmu_buf_t *l_dbuf;
	zap_leaf_phys_t *l_phys;
} zap_leaf_t;


typedef struct zap_entry_handle {
	/* below is set by zap_leaf.c and is public to zap.c */
	uint64_t zeh_num_integers;
	uint64_t zeh_hash;
	uint32_t zeh_cd;
	uint8_t zeh_integer_size;

	/* below is private to zap_leaf.c */
	uint16_t zeh_fakechunk;
	uint16_t *zeh_chunkp;
	zap_leaf_t *zeh_head_leaf;
	zap_leaf_t *zeh_found_leaf;
} zap_entry_handle_t;

/*
 * Return a handle to the named entry, or ENOENT if not found.  The hash
 * value must equal zap_hash(name).
 */
extern int zap_leaf_lookup(zap_leaf_t *l,
	const char *name, uint64_t h, zap_entry_handle_t *zeh);

/*
 * Return a handle to the entry with this hash+cd, or the entry with the
 * next closest hash+cd.
 */
extern int zap_leaf_lookup_closest(zap_leaf_t *l,
    uint64_t hash, uint32_t cd, zap_entry_handle_t *zeh);

/*
 * Read the first num_integers in the attribute.  Integer size
 * conversion will be done without sign extension.  Return EINVAL if
 * integer_size is too small.  Return EOVERFLOW if there are more than
 * num_integers in the attribute.
 */
extern int zap_entry_read(const zap_entry_handle_t *zeh,
	uint8_t integer_size, uint64_t num_integers, void *buf);

extern int zap_entry_read_name(const zap_entry_handle_t *zeh,
	uint16_t buflen, char *buf);

/*
 * Replace the value of an existing entry.
 *
 * zap_entry_update may fail if it runs out of space (ENOSPC).
 */
extern int zap_entry_update(zap_entry_handle_t *zeh,
	uint8_t integer_size, uint64_t num_integers, const void *buf);

/*
 * Remove an entry.
 */
extern void zap_entry_remove(zap_entry_handle_t *zeh);

/*
 * Create an entry. An equal entry must not exist, and this entry must
 * belong in this leaf (according to its hash value).  Fills in the
 * entry handle on success.  Returns 0 on success or ENOSPC on failure.
 */
extern int zap_entry_create(zap_leaf_t *l,
	const char *name, uint64_t h, uint32_t cd,
	uint8_t integer_size, uint64_t num_integers, const void *buf,
	zap_entry_handle_t *zeh);

/*
 * Other stuff.
 */

extern void zap_leaf_init(zap_leaf_t *l);
extern void zap_leaf_byteswap(zap_leaf_phys_t *buf);

extern zap_leaf_t *zap_leaf_split(struct zap *zap, zap_leaf_t *l, dmu_tx_t *tx);

extern int zap_leaf_merge(zap_leaf_t *l, zap_leaf_t *sibling);

extern zap_leaf_t *zap_leaf_chainmore(zap_leaf_t *l, zap_leaf_t *nl);

extern int zap_leaf_advance(zap_leaf_t *l, zap_cursor_t *zc);

extern void zap_stats_leaf(zap_t *zap, zap_leaf_t *l, zap_stats_t *zs);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ZAP_LEAF_H */
