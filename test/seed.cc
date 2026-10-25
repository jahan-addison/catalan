/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <doctest/doctest.h> // for ResultBuilder, CHECK, TEST_CASE

#include "fixture.hh"          // for Temp_Database_Path
#include <catalan/api.hh>      // for is_post_id
#include <catalan/examples.hh> // for k_euler_identity, k_basel_problem
#include <catalan/manage.hh>   // for Database, Post, list_posts, Order
#include <catalan/seed.hh>     // for seed_examples, k_basel_problem_id
#include <optional>            // for nullopt
#include <string>              // for basic_string, string

using catalan::test::Temp_Database_Path;

TEST_CASE("Database::is_new")
{
    Temp_Database_Path temp{};
    {
        catalan::orm::Database db{ temp.string() };
        CHECK(db.is_new());
    }
    catalan::orm::Database db{ temp.string() };
    CHECK_FALSE(db.is_new());
}

TEST_CASE("seed_examples")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    db.with_storage(
        [](auto& storage) { catalan::orm::seed_examples(storage, 1'000); });

    SUBCASE("both posts, at their fixed ids, with the example files' bodies")
    {
        auto const euler = db.with_storage([](auto& storage) {
            return catalan::orm::get_post(
                storage, std::string{ catalan::orm::k_euler_identity_id });
        });
        auto const basel = db.with_storage([](auto& storage) {
            return catalan::orm::get_post(
                storage, std::string{ catalan::orm::k_basel_problem_id });
        });
        REQUIRE(euler);
        REQUIRE(basel);
        CHECK_EQ(euler->title, "Euler's identity");
        CHECK_EQ(basel->title, "The Basel problem");
        CHECK_EQ(euler->body, catalan::examples::k_euler_identity);
        CHECK_EQ(basel->body, catalan::examples::k_basel_problem);
        CHECK_LT(euler->created_at, basel->created_at);
        CHECK_FALSE(euler->unlisted);
    }

    SUBCASE("last in listings, Euler last of all")
    {
        db.with_storage([](auto& storage) {
            catalan::orm::Post post{};
            post.id = "7HcPq2mVa0Rn";
            post.created_at = 5'000;
            post.body = "a newer post";
            catalan::orm::insert_post(storage, post);
        });

        auto const page = db.with_storage([](auto& storage) {
            return catalan::orm::list_posts(
                storage, 20, catalan::orm::Order::desc, std::nullopt);
        });
        REQUIRE_EQ(page.posts.size(), 3);
        CHECK_EQ(page.posts[0].id, "7HcPq2mVa0Rn");
        CHECK_EQ(page.posts[1].id, catalan::orm::k_basel_problem_id);
        CHECK_EQ(page.posts[2].id, catalan::orm::k_euler_identity_id);
    }
}

TEST_CASE("example ids are valid post ids")
{
    static_assert(catalan::api::is_post_id(catalan::orm::k_euler_identity_id));
    static_assert(catalan::api::is_post_id(catalan::orm::k_basel_problem_id));
}
