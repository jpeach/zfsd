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

#include <catch.hpp>
#include <spl/types.h>
#include <spl/nvpair.h>
#include <sys/spa.h>
#include <sys/rrwlock.h>

extern uint_t rrw_tsd_key;

// See zfd_ioctl.c::_init() for ZFS initialization ordering.
struct scoped_spa_fixture
{
    scoped_spa_fixture() {
        // Force verbose ZFS debug logging.
	    zfs_flags = ~0;
        setenv("ZFS_DEBUG", "on,long", 0 /* overwrite */);

        system_taskq_init();
        spa_init(FREAD | FWRITE);
        tsd_create(&rrw_tsd_key, rrw_tsd_destroy);
    }

    ~scoped_spa_fixture() {
        spa_fini();
        tsd_destroy(&rrw_tsd_key);
        system_taskq_fini();

        for (auto fd : descriptors) {
            close(fd);
        }

        for (auto path : paths) {
            unlink(path.c_str());
        }
    }

    bool makefile(const char * path, uint64_t nbytes)
    {
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
        int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, mode);
        if (fd == -1) {
            return false;
        }

        descriptors.push_back(fd);
        paths.push_back(path);

        // Make a best effort to preallocate the file. This can take a while
        // if glibc emulates posix_fallocate by writing a byte at a time.
        ftruncate(fd, nbytes);
        posix_fallocate(fd, 0, nbytes);
        return true;
    }

    // Allocate a nvlist containing a single file vdev.
    nvlist_t * filedev(const char * path)
    {
        nvlist_t * nv = fnvlist_alloc();
        char * abs = realpath(path, nullptr);

        fnvlist_add_string(nv, ZPOOL_CONFIG_PATH, abs);
        fnvlist_add_string(nv, ZPOOL_CONFIG_TYPE, VDEV_TYPE_FILE);
        fnvlist_add_uint64(nv, ZPOOL_CONFIG_IS_LOG, B_FALSE);

        free(abs);
        return nv;
    }

    std::vector<int> descriptors;
    std::vector<std::string> paths;
};

TEST_CASE("Basic spa_create()", "[spa]")
{
    nvlist_t * nvroot = nullptr;
    nvlist_t * props = nullptr;
    nvlist_t * zplprops = nullptr;

    scoped_spa_fixture spa;

    // Make a pool with one file vdev.
    SECTION("create a default pool") {
        nvlist_t * vdev;

        REQUIRE(spa.makefile("spa.0", SPA_MINDEVSIZE));
        vdev = spa.filedev("spa.0");

        nvroot = fnvlist_alloc();

        fnvlist_add_string(nvroot, ZPOOL_CONFIG_TYPE, VDEV_TYPE_ROOT);
        fnvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN, &vdev, 1);

        REQUIRE(spa_create("test.0", nvroot, props, zplprops) == 0);
        nvlist_free(nvroot);
        nvlist_free(vdev);
    }
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
