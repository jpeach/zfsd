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
#include <lua.hpp>
#include <ck_pr.h>
#include <libnvpair.h>
#include <string.h>

TEST_CASE("Link libck", "[link]")
{
    int value = 0;

    ck_pr_inc_int(&value);
    REQUIRE(value == 1);
}

TEST_CASE("Link LuaJit", "[link]")
{
    lua_State * lua;

    lua = luaL_newstate();
    REQUIRE(lua != NULL);

    lua_close(lua);
}

TEST_CASE("Link libnvpair", "[link]")
{
    char json[BUFSIZ];

    nvlist_t * nv;
    FILE * fp;

    fp = fmemopen(json, sizeof(json), "w");
    REQUIRE(fp != nullptr);

    nv = fnvlist_alloc();
    REQUIRE(nv != nullptr);

    REQUIRE(nvlist_add_boolean_value(nv, "bool", B_TRUE) == ESUCCESS);
    REQUIRE(nvlist_print_json(fp, nv) == 0);

    fclose(fp);

    REQUIRE(::strcmp("{\"bool\":true}", json) == 0);

    nvlist_free(nv);
}

/* vim: set sts=4 sw=4 ts=4 tw=79 et: */
