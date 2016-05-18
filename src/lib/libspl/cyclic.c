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

#include <spl/types.h>
#include <spl/cyclic.h>

/* See spa_add() for cyclic timer usage ... */

cyclic_id_t
cyclic_add(cyc_handler_t * handler, cyc_time_t * when)
{
    return CYCLIC_NONE;
}

int
cyclic_reprogram(cyclic_id_t id, hrtime_t expiration)
{
    return 1; /* success! */
}

void
cyclic_remove(cyclic_id_t id)
{
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
