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
 * Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef SYSEVENT_H_BF25A668_C091_48AD_8C2C_826BE825DE6A
#define SYSEVENT_H_BF25A668_C091_48AD_8C2C_826BE825DE6A

#include <spl/nvpair.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Event allocation/enqueuing sleep/nosleep flags
 */
#define SE_SLEEP                0
#define SE_NOSLEEP              1

/* Internal data types */
#define SE_DATA_TYPE_BYTE       DATA_TYPE_BYTE
#define SE_DATA_TYPE_INT16      DATA_TYPE_INT16
#define SE_DATA_TYPE_UINT16     DATA_TYPE_UINT16
#define SE_DATA_TYPE_INT32      DATA_TYPE_INT32
#define SE_DATA_TYPE_UINT32     DATA_TYPE_UINT32
#define SE_DATA_TYPE_INT64      DATA_TYPE_INT64
#define SE_DATA_TYPE_UINT64     DATA_TYPE_UINT64
#define SE_DATA_TYPE_STRING     DATA_TYPE_STRING
#define SE_DATA_TYPE_BYTES      DATA_TYPE_BYTE_ARRAY
#define SE_DATA_TYPE_TIME       DATA_TYPE_HRTIME

/* Opaque sysevent_t data type */
typedef void *sysevent_t;

/* Opaque channel bind data type */
typedef void evchan_t;

/* sysevent attribute list */
typedef nvlist_t sysevent_attr_list_t;

/* sysevent attribute name-value pair */
typedef nvpair_t sysevent_attr_t;

/* Unique event identifier */
typedef struct sysevent_id {
        uint64_t eid_seq;
        hrtime_t eid_ts;
} sysevent_id_t;

/* Event attribute value structures */
typedef struct sysevent_bytes {
        int32_t size;
        uchar_t *data;
} sysevent_bytes_t;

typedef struct sysevent_value {
        int32_t         value_type;             /* data type */
        union {
                uchar_t         sv_byte;
                int16_t         sv_int16;
                uint16_t        sv_uint16;
                int32_t         sv_int32;
                uint32_t        sv_uint32;
                int64_t         sv_int64;
                uint64_t        sv_uint64;
                hrtime_t        sv_time;
                char            *sv_string;
                sysevent_bytes_t        sv_bytes;
        } value;
} sysevent_value_t;

#define sysevent_alloc(a, b, c, d) (NULL)
#define sysevent_free(ev)
#define sysevent_add_attr(l, a, b, c) (1)
#define sysevent_attach_attributes(e, l) (1)

#define sysevent_free_attr(ev)

#define log_sysevent(a, b, c) (0)

#ifdef  __cplusplus
}
#endif

#endif /* SYSEVENT_H_BF25A668_C091_48AD_8C2C_826BE825DE6A */
