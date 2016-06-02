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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 * Copyright 2014 Joyent, Inc.  All rights reserved.
 */

/*
 * Implementations of the functions described in vsnprintf(3C) and string(3C),
 * for use by the kernel, the standalone, and kmdb.  Unless otherwise specified,
 * these functions match the section 3C manpages.
 */

#include <sys/types.h>
#if !defined(__zfsd__)
#include <sys/null.h>
#endif /* !defined(__zfsd__) */
#include <sys/varargs.h>

#if defined(_KERNEL)
#include <sys/systm.h>
#include <sys/debug.h>
#elif !defined(_BOOT)
#include <string.h>
#endif

#if !defined(__zfsd__)
#include "memcpy.h"
#endif /* !defined(__zfsd__) */
#include "string.h"

size_t
strlcat(char *dst, const char *src, size_t dstsize)
{
	char *df = dst;
	size_t left = dstsize;
	size_t l1;
	size_t l2 = strlen(src);
	size_t copied;

	while (left-- != 0 && *df != '\0')
		df++;
	/*LINTED: possible ptrdiff_t overflow*/
	l1 = (size_t)(df - dst);
	if (dstsize == l1)
		return (l1 + l2);

	copied = l1 + l2 >= dstsize ? dstsize - l1 - 1 : l2;
	bcopy(src, dst + l1, copied);
	dst[l1+copied] = '\0';
	return (l1 + l2);
}

size_t
strlcpy(char *dst, const char *src, size_t len)
{
	size_t slen = strlen(src);
	size_t copied;

	if (len == 0)
		return (slen);

	if (slen >= len)
		copied = len - 1;
	else
		copied = slen;
	bcopy(src, dst, copied);
	dst[copied] = '\0';
	return (slen);
}

int
ddi_strtoul(const char *str, char **endptr, int base, unsigned long *result)
{
        *result = strtoul(str, endptr, base);
        if (*result == 0)
                return errno;
        return 0;
}

int
ddi_strtoull(const char *str, char **endptr, int base, unsigned long long *result)
{
        *result = strtoull(str, endptr, base);
        if (*result == 0)
                return errno;
        return 0;
}
