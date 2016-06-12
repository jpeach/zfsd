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

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

/* From usr/src/uts/common/os/move.c ... */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>

#if defined(__zfsd__)
#include <spl/debug.h>
#endif

#if !defined(__zfsd__)
#include <sys/errno.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <vm/as.h>
#include <vm/page.h>

#include <sys/dcopy.h>

int64_t uioa_maxpoll = -1;	/* <0 = noblock, 0 = block, >0 = block after */
#define	UIO_DCOPY_CHANNEL	0
#define	UIO_DCOPY_CMD		1
#endif /* !defined(__zfsd__) */

/*
 * Move "n" bytes at byte address "p"; "rw" indicates the direction
 * of the move, and the I/O parameters are provided in "uio", which is
 * update to reflect the data which was moved.  Returns 0 on success or
 * a non-zero errno on failure.
 */
int
uiomove(void *p, size_t n, enum uio_rw rw, struct uio *uio)
{
	struct iovec *iov;
	ulong_t cnt;
	int error;

	while (n && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0l) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}

#if defined(__zfsd__)

		if (rw == UIO_READ) {
			memcpy(iov->iov_base, p, cnt);
		} else {
			VERIFY3(rw, ==, UIO_WRITE);
			memcpy(p, iov->iov_base, cnt);
		}

#else /* defined(__zfsd__) */

		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (rw == UIO_READ) {
				error = xcopyout_nta(p, iov->iov_base, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			} else {
				error = xcopyin_nta(iov->iov_base, p, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			}

			if (error)
				return (error);
			break;

		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				error = kcopy_nta(p, iov->iov_base, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			else
				error = kcopy_nta(iov->iov_base, p, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			if (error)
				return (error);
			break;
		}

#endif /* defined(__zfsd__) */

		iov->iov_base += cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		p = (caddr_t)p + cnt;
		n -= cnt;
	}
	return (0);
}

/*
 * same as uiomove() but doesn't modify uio structure.
 * return in cbytes how many bytes were copied.
 */
int
uiocopy(void *p, size_t n, enum uio_rw rw, struct uio *uio, size_t *cbytes)
{
	struct iovec *iov;
	ulong_t cnt;
	int error;
	int iovcnt;

	iovcnt = uio->uio_iovcnt;
	*cbytes = 0;

	for (iov = uio->uio_iov; n && iovcnt; iov++, iovcnt--) {
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0)
			continue;

#if defined(__zfsd__)

		if (rw == UIO_READ) {
			memcpy(iov->iov_base, p, cnt);
		} else {
			VERIFY3(rw, ==, UIO_WRITE);
			memcpy(p, iov->iov_base, cnt);
		}

#else /* defined(__zfsd__) */

		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
		case UIO_USERISPACE:
			if (rw == UIO_READ) {
				error = xcopyout_nta(p, iov->iov_base, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			} else {
				error = xcopyin_nta(iov->iov_base, p, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			}

			if (error)
				return (error);
			break;

		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				error = kcopy_nta(p, iov->iov_base, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			else
				error = kcopy_nta(iov->iov_base, p, cnt,
				    (uio->uio_extflg & UIO_COPY_CACHED));
			if (error)
				return (error);
			break;
		}

#endif /* defined(__zfsd__) */

		p = (caddr_t)p + cnt;
		n -= cnt;
		*cbytes += cnt;
	}
	return (0);
}

/*
 * Drop the next n chars out of *uiop.
 */
void
uioskip(uio_t *uiop, size_t n)
{
	if (n > uiop->uio_resid)
		return;
	while (n != 0) {
		register iovec_t	*iovp = uiop->uio_iov;
		register size_t		niovb = MIN(iovp->iov_len, n);

		if (niovb == 0) {
			uiop->uio_iov++;
			uiop->uio_iovcnt--;
			continue;
		}
		iovp->iov_base += niovb;
		uiop->uio_loffset += niovb;
		iovp->iov_len -= niovb;
		uiop->uio_resid -= niovb;
		n -= niovb;
	}
}
