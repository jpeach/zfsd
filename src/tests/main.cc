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

#define CATCH_CONFIG_RUNNER
#include <catch.hpp>
#include <spl/list.h>
#include <sys/zfs_debug.h>

int main(int argc, char* const argv[])
{
    // Enable all debugging flags for the tests.
    zfs_flags = ~0;

    // Turn on all debug logging.
    setenv("ZFS_DEBUG", "on,long", 0 /* overwrite */);

    return Catch::Session().run(argc, argv);
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
