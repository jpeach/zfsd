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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2013 Saso Kiselkov. All rights reserved.
 */
#include <sys/zfs_context.h>
#include <sys/zio.h>
#if defined(__zfsd__)
#include <openssl/sha.h>
#else
#include <sys/sha2.h>
#endif /* defined(__zfsd__) */

/*ARGSUSED*/
void
zio_checksum_SHA256(const void *buf, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
#if defined(__zfsd__)
	SHA256_CTX ctx;
	zio_cksum_t tmp;

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, buf, size);
	SHA256_Final((uint8_t *)&tmp, &ctx);

	CTASSERT(sizeof(zio_cksum_t) == SHA256_DIGEST_LENGTH);
#else
	SHA2_CTX ctx;
	zio_cksum_t tmp;

	SHA2Init(SHA256, &ctx);
	SHA2Update(&ctx, buf, size);
	SHA2Final(&tmp, &ctx);
#endif /* defined(__zfsd__) */

	/*
	 * A prior implementation of this function had a
	 * private SHA256 implementation always wrote things out in
	 * Big Endian and there wasn't a byteswap variant of it.
	 * To preseve on disk compatibility we need to force that
	 * behaviour.
	 */
	zcp->zc_word[0] = BE_64(tmp.zc_word[0]);
	zcp->zc_word[1] = BE_64(tmp.zc_word[1]);
	zcp->zc_word[2] = BE_64(tmp.zc_word[2]);
	zcp->zc_word[3] = BE_64(tmp.zc_word[3]);
}

/*ARGSUSED*/
void
zio_checksum_SHA512_native(const void *buf, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
#if defined(__zfsd__)
	SHA512_CTX ctx;
	union {
		struct {
			zio_cksum_t first;
			zio_cksum_t second;
		} digest;
		uint8_t bytes[SHA512_DIGEST_LENGTH];
	} digest;

	SHA512_Init(&ctx);
	SHA512_Update(&ctx, buf, size);
	SHA512_Final(digest.bytes, &ctx);

	CTASSERT(sizeof(digest) == SHA512_DIGEST_LENGTH);

	/* XXX True SHA 512/256 changes the initialization vector as
	 * well as truncating the result. AFAICT OpenSSL does not support
	 * this, so until then this would result in an incompatible on-disk
	 * checksum.
	 */
	*zcp = digest.digest.second;
#else
	SHA2_CTX	ctx;

	SHA2Init(SHA512_256, &ctx);
	SHA2Update(&ctx, buf, size);
	SHA2Final(zcp, &ctx);
#endif /* defined(__zfsd__) */
}

/*ARGSUSED*/
void
zio_checksum_SHA512_byteswap(const void *buf, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
	zio_cksum_t	tmp;

	zio_checksum_SHA512_native(buf, size, ctx_template, &tmp);
	zcp->zc_word[0] = BSWAP_64(tmp.zc_word[0]);
	zcp->zc_word[1] = BSWAP_64(tmp.zc_word[1]);
	zcp->zc_word[2] = BSWAP_64(tmp.zc_word[2]);
	zcp->zc_word[3] = BSWAP_64(tmp.zc_word[3]);
}
