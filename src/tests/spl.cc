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
#include <spl/atomic.h>
#include <spl/rwlock.h>
#include <spl/random.h>
#include <spl/byteorder.h>
#include <string.h>

// Basic atomic ops tests. We are not testing the atomicity here, just
// that the APIs return the expected values (ie. the pre- or post- value.
TEST_CASE("Basic 32bit atomic ops", "[spl]")
{
    uint32_t start = 0;

    REQUIRE(atomic_inc_32_nv(&start) == 1);
    REQUIRE(atomic_dec_32_nv(&start) == 0);

    REQUIRE(atomic_add_32_nv(&start, 100) == 100);
    REQUIRE(atomic_add_32_nv(&start, -100) == 0);

    atomic_add_32(&start, 59);
    REQUIRE(start == 59);

    atomic_add_32(&start, -59);
    REQUIRE(start == 0);
}

TEST_CASE("Basic 64bit atomic ops", "[spl]")
{
    uint64_t start = 0;

    REQUIRE(atomic_inc_64_nv(&start) == 1);
    REQUIRE(atomic_dec_64_nv(&start) == 0);

    REQUIRE(atomic_add_64_nv(&start, 100) == 100);
    REQUIRE(atomic_add_64_nv(&start, -100) == 0);

    atomic_add_64(&start, 59);
    REQUIRE(start == 59);

    atomic_add_64(&start, -59);
    REQUIRE(start == 0);
}

TEST_CASE("Basic rwlock API", "[spl]")
{
    krwlock_t rw;

    rw_init(&rw, NULL, RW_DEFAULT, NULL);

    rw_enter(&rw, RW_READER);
    REQUIRE(RW_READ_HELD(&rw));
    REQUIRE(RW_WRITE_HELD(&rw) == false);
    REQUIRE(rw_tryenter(&rw, RW_WRITER) == false);

    rw_exit(&rw);
    REQUIRE(RW_READ_HELD(&rw) == false);
    REQUIRE(RW_LOCK_HELD(&rw) == false);

    rw_enter(&rw, RW_WRITER);
    REQUIRE(RW_READ_HELD(&rw) == false);
    REQUIRE(RW_WRITE_HELD(&rw) == true);

    REQUIRE(rw_tryenter(&rw, RW_WRITER) == false);
    REQUIRE(rw_tryenter(&rw, RW_READER) == false);

    rw_exit(&rw);
    REQUIRE(RW_LOCK_HELD(&rw) == false);
    REQUIRE(RW_WRITE_HELD(&rw) == false);

    rw_destroy(&rw);
}

TEST_CASE("Basic random bytes", "[spl]")
{
    // Make a large oddly-sized buffer, since getrandom(2) tells us
    // that reads of up to 256 bytes will always return as  many bytes
    // as requested and will not be interrupted by signals.

    uint8_t buf[512 * 1019];
    uint8_t zero[512 * 1019];

    memset(buf, 0, sizeof(buf));
    memset(zero, 0, sizeof(buf));

    REQUIRE(memcmp(buf, zero, sizeof(buf)) == 0);
    REQUIRE(random_get_pseudo_bytes(buf, sizeof(buf)) == 0);
    REQUIRE(memcmp(buf, zero, sizeof(buf)) != 0);
}

TEST_CASE("Basic byte order", "[spl]")
{
    const uint8_t bytes[] = {
        0, // Misalignment padding.
        0x13, 0x88, // Big endian 5000.
        0x10, 0x00, // Little endian 16.
        0x58, 0x7A, 0xBC, 0x94 // Big endian 1484438676.
    };

    REQUIRE(BE_IN16(&bytes[1]) == 5000);
    REQUIRE(LE_IN16(&bytes[3]) == 16);
    REQUIRE(BE_IN32(&bytes[5]) == 1484438676);
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
