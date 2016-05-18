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
 */

#ifndef _SYS_CYCLIC_H
#define	_SYS_CYCLIC_H

#ifdef	__cplusplus
extern "C" {
#endif

#define	CY_LOW_LEVEL		0
#define	CY_LOCK_LEVEL		1
#define	CY_HIGH_LEVEL		2
#define	CY_SOFT_LEVELS		2
#define	CY_LEVELS		3

typedef uintptr_t cyclic_id_t;
typedef int cyc_index_t;
typedef int cyc_cookie_t;
typedef uint16_t cyc_level_t;
typedef void (*cyc_func_t)(void *);
typedef void *cyb_arg_t;

#define	CYCLIC_NONE		((cyclic_id_t)0)

typedef struct cyc_handler {
	cyc_func_t cyh_func;
	void *cyh_arg;
	cyc_level_t cyh_level;
} cyc_handler_t;

typedef struct cyc_time {
	hrtime_t cyt_when;
	hrtime_t cyt_interval;
} cyc_time_t;

#define	CY_INFINITY	INT64_MAX

extern cyclic_id_t cyclic_add(cyc_handler_t *, cyc_time_t *);
extern void cyclic_remove(cyclic_id_t);
extern int cyclic_reprogram(cyclic_id_t, hrtime_t);
extern hrtime_t cyclic_getres();

extern void cyclic_suspend();
extern void cyclic_resume();

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CYCLIC_H */
