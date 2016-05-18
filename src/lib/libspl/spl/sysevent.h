/** @file
 *
 *  A brief file description
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef SYSEVENT_H_BF25A668_C091_48AD_8C2C_826BE825DE6A
#define SYSEVENT_H_BF25A668_C091_48AD_8C2C_826BE825DE6A

#ifdef  __cplusplus
extern "C" {
#endif

/* Opaque stub types for sysevent API ... */
typedef struct sysevent_ sysevent_t;
typedef struct sysevent_id sysevent_id_t;
typedef struct sysevent_attr sysevent_attr_list_t;
typedef struct sysevent_value sysevent_value_t;

#define sysevent_alloc(a, b, c, d) (NULL)
#define sysevent_add_attr(l, a, b, c) (1)
#define sysevent_attach_attributes(e, l) (1)

#define sysevent_free_attr(ev)

#define log_sysevent(a, b, c)

#ifdef  __cplusplus
}
#endif

#endif /* SYSEVENT_H_BF25A668_C091_48AD_8C2C_826BE825DE6A */
