/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <doctest/doctest.h> // for ResultBuilder, CHECK, TEST_CASE

#include <catalan/api.hh> // for make_post_id, is_post_id, k_id_length
#include <cstddef>        // for size_t
#include <string>         // for basic_string, string
#include <unordered_set>  // for unordered_set

TEST_CASE("make_post_id")
{
    auto id = catalan::api::make_post_id();
    CHECK_EQ(id.size(), catalan::api::k_id_length);
    CHECK(catalan::api::is_post_id(id));

    std::unordered_set<std::string> seen{};
    for (std::size_t i = 0; i < 10'000; i++)
        seen.insert(catalan::api::make_post_id());
    CHECK_EQ(seen.size(), 10'000);
}

static_assert(catalan::api::is_post_id("7HcPq2mVa0Rn"));
static_assert(not catalan::api::is_post_id("7HcPq2mVa0R-"));

TEST_CASE("is_post_id")
{
    CHECK(catalan::api::is_post_id("7HcPq2mVa0Rn"));
    CHECK(catalan::api::is_post_id("000000000000"));

    CHECK_FALSE(catalan::api::is_post_id(""));
    CHECK_FALSE(catalan::api::is_post_id("7HcPq2mVa0R"));
    CHECK_FALSE(catalan::api::is_post_id("7HcPq2mVa0Rn0"));
    CHECK_FALSE(catalan::api::is_post_id("7HcPq2mVa0R-"));
    CHECK_FALSE(catalan::api::is_post_id("7HcPq2mVa0R/"));
    CHECK_FALSE(catalan::api::is_post_id("../../etc/pa"));
}
