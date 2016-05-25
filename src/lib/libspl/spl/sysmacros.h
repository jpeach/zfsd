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

#ifndef SYSMACROS_H_EDA94979_B460_49CA_96BF_AED39462B64F
#define SYSMACROS_H_EDA94979_B460_49CA_96BF_AED39462B64F

#include <unistd.h>
#include <limits.h> /* For NAME_MAX. */

#ifdef  __cplusplus
extern "C" {
#endif

/* From Illumos usr/src/uts/common/sys/param.h ... */
#define MAXBSIZE        8192
#define DEV_BSIZE       512
#define DEV_BSHIFT      9               /* log2(DEV_BSIZE) */

/* _NOTE(x) is a tools annotation, expand it to nothing. */
#define _NOTE(args)

#define PAGESIZE getpagesize()
#define physmem sysconf(_SC_PHYS_PAGES)

#define MAXUID          2147483647      /* max user id */

#define UID_NOBODY      60001   /* user ID no body */
#define GID_NOBODY      UID_NOBODY

/* Side-effect free version of MIN(). */
#undef MIN
#define MIN(lhs, rhs) ({ \
    typeof(lhs) _a = (lhs); \
    typeof(rhs) _b = (rhs); \
    _a < _b ? _a : _b; \
})

/* Side-effect free version of MAX(). */
#undef MAX
#define MAX(lhs, rhs) ({ \
    typeof(lhs) _a = (lhs); \
    typeof(rhs) _b = (rhs); \
    _a > _b ? _a : _b; \
})

#define ABS(val) ({ \
    typeof(val) _v = (val); \
    _v < 0 ? -_v : _v; \
})

#define COUNTOF(array) (sizeof(array) / sizeof((array)[0]))

#define MAXNAMELEN NAME_MAX

/*
 * Macro to determine if value is a power of 2
 */
#define ISP2(x)         (((x) & ((x) - 1)) == 0)

/*
 * Macro for checking power of 2 address alignment.
 */
#define IS_P2ALIGNED(v, a) ((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)

/*
 * return x rounded up to an align boundary
 * eg, P2ROUNDUP(0x1234, 0x100) == 0x1300 (0x13*align)
 * eg, P2ROUNDUP(0x5600, 0x100) == 0x5600 (0x56*align)
 */
#define P2ROUNDUP(x, align)             (-(-(x) & -(align)))

#define P2ROUNDUP_TYPED(x, align, type) \
        (-(-(type)(x) & -(type)(align)))

/*
 * return x rounded down to an align boundary
 * eg, P2ALIGN(1200, 1024) == 1024 (1*align)
 * eg, P2ALIGN(1024, 1024) == 1024 (1*align)
 * eg, P2ALIGN(0x1234, 0x100) == 0x1200 (0x12*align)
 * eg, P2ALIGN(0x5600, 0x100) == 0x5600 (0x56*align)
 */
#define P2ALIGN(x, align)               ((x) & -(align))

/*
 * return x % (mod) align
 * eg, P2PHASE(0x1234, 0x100) == 0x34 (x-0x12*align)
 * eg, P2PHASE(0x5600, 0x100) == 0x00 (x-0x56*align)
 */
#define P2PHASE(x, align)               ((x) & ((align) - 1))

/*
 * return how much space is left in this block (but if it's perfectly
 * aligned, return 0).
 * eg, P2NPHASE(0x1234, 0x100) == 0xcc (0x13*align-x)
 * eg, P2NPHASE(0x5600, 0x100) == 0x00 (0x56*align-x)
 */
#define P2NPHASE(x, align)              (-(x) & ((align) - 1))

/*
 * return TRUE if adding len to off would cause it to cross an align
 * boundary.
 * eg, P2BOUNDARY(0x1234, 0xe0, 0x100) == TRUE (0x1234 + 0xe0 == 0x1314)
 * eg, P2BOUNDARY(0x1234, 0x50, 0x100) == FALSE (0x1234 + 0x50 == 0x1284)
 */
#define P2BOUNDARY(off, len, align) \
        (((off) ^ ((off) + (len) - 1)) > (align) - 1)

/*
 * Typed version of the P2* macros.  These macros should be used to ensure
 * that the result is correctly calculated based on the data type of (x),
 * which is passed in as the last argument, regardless of the data
 * type of the alignment.  For example, if (x) is of type uint64_t,
 * and we want to round it up to a page boundary using "PAGESIZE" as
 * the alignment, we can do either
 *      P2ROUNDUP(x, (uint64_t)PAGESIZE)
 * or
 *      P2ROUNDUP_TYPED(x, PAGESIZE, uint64_t)
 */
#define P2ALIGN_TYPED(x, align, type)   \
        ((type)(x) & -(type)(align))
#define P2PHASE_TYPED(x, align, type)   \
        ((type)(x) & ((type)(align) - 1))
#define P2NPHASE_TYPED(x, align, type)  \
        (-(type)(x) & ((type)(align) - 1))
#define P2ROUNDUP_TYPED(x, align, type) \
        (-(-(type)(x) & -(type)(align)))
#define P2END_TYPED(x, align, type)     \
        (-(~(type)(x) & -(type)(align)))
#define P2PHASEUP_TYPED(x, align, phase, type)  \
        ((type)(phase) - (((type)(phase) - (type)(x)) & -(type)(align)))
#define P2CROSS_TYPED(x, y, align, type)        \
        (((type)(x) ^ (type)(y)) > (type)(align) - 1)
#define P2SAMEHIGHBIT_TYPED(x, y, type) \
        (((type)(x) ^ (type)(y)) < ((type)(x) & (type)(y)))

#define issig(x) (false)

#ifdef  __cplusplus
}
#endif

#endif /* SYSMACROS_H_EDA94979_B460_49CA_96BF_AED39462B64F */
