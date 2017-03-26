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

#ifndef INIT_H_B3A0DE15_0420_4D89_9920_0316BCA65271
#define INIT_H_B3A0DE15_0420_4D89_9920_0316BCA65271

#ifdef  __cplusplus
extern "C" {
#endif

// zfsd_init_phenom initializes libphenom.
void zfsd_init_phenom(void);

// zfsd_init_zfs initializes libphenom.
void zfsd_init_zfs(void);

// zfsd_run drops into the main event loop and never returns.
void zfsd_run(void);

#ifdef  __cplusplus
}
#endif

#endif /* INIT_H_B3A0DE15_0420_4D89_9920_0316BCA65271 */
