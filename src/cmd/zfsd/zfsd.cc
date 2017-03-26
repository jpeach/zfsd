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

#include <string>

#include <getopt.h>
#include <sysexits.h>

#include <phenom/defs.h>
#include <phenom/log.h>

#include <spl/types.h>
#include <spl/nvpair.h>
#include <sys/spa.h>
#include <sys/rrwlock.h>

#include "init.h"

static const std::string usage = R"(
Usage: zfsd [OPTION]...
ZFS storage daemon

Options:
  --debug   Enable verbose debug logging
)";

int
main(int argc, const char ** argv)
{
    static const struct option options[] = {
        {"debug", no_argument, nullptr, 'd' },
        {nullptr, 0, nullptr, '\0' }
    };

    zfsd_init_phenom();

    for (;;) {
        int opt = getopt_long(argc, (char *const *)argv, "", options, nullptr);

        switch (opt) {
        case 'd':
            // Force verbose ZFS debug logging.
            zfs_flags = ~0;
            setenv("ZFS_DEBUG", "on,long", 0 /* overwrite */);

            // Enable libphenom debug logging.
            ph_log_level_set(PH_LOG_DEBUG);
            break;

        case -1:
            // Option parsing done.
            break;

        default:
            printf("%s", usage.c_str());
            return EX_USAGE;
        }

        if (opt == -1) {
            break;
        }
    }

    ph_log(PH_LOG_DEBUG, "starting ZFS");
    zfsd_init_zfs();

    ph_log(PH_LOG_DEBUG, "ready");
    zfsd_run();
    return EX_OK;
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
