/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <doctest/doctest.h> // for ResultBuilder, CHECK, TEST_CASE

#include "fixture.hh"              // for Temp_Database_Path
#include <algorithm>               // for transform
#include <catalan/manage.hh>       // for Database, Post, list_posts, Cursor
#include <cstddef>                 // for size_t
#include <cstdint>                 // for int64_t
#include <iterator>                // for back_inserter
#include <optional>                // for optional, nullopt
#include <set>                     // for set
#include <sqlite_orm/sqlite_orm.h> // for make_storage, make_table
#include <stdexcept>               // for runtime_error
#include <string>                  // for basic_string, string, to_string
#include <system_error>            // for system_error
#include <utility>                 // for move
#include <vector>                  // for vector

namespace {

using catalan::orm::Post;
using catalan::test::Temp_Database_Path;

Post make_post(std::string id,
    std::int64_t created_at = 1'761'275'700'000,
    std::optional<std::string> parent_id = std::nullopt)
{
    Post post{};
    post.id = std::move(id);
    post.created_at = created_at;
    post.parent_id = std::move(parent_id);
    post.body = "$$e^{i\\pi} + 1 = 0$$";
    return post;
}

/**
 * @brief A 12 character base62 id from a number, ordered like the number
 *
 *  Example:
 *
 *    numbered_id(7); // "000000000007"
 */
std::string numbered_id(int n)
{
    auto id = std::to_string(n);
    return std::string(12 - id.size(), '0') + id;
}

/**
 * @brief Walk every page of the listing, return the ids in order
 */
std::vector<std::string> walk(catalan::orm::Database& db,
    std::size_t limit,
    catalan::orm::Order order)
{
    std::vector<std::string> ids{};
    std::optional<catalan::orm::Cursor> after{};
    do {
        auto const page = db.with_storage([&](auto& storage) {
            return catalan::orm::list_posts(storage, limit, order, after);
        });
        CHECK_LE(page.posts.size(), limit);
        std::transform(page.posts.begin(),
            page.posts.end(),
            std::back_inserter(ids),
            [](auto const& post) { return post.id; });
        after = page.next;
    } while (after);
    return ids;
}

/**
 * @brief Schema version 1, before idempotency_keys
 */
auto make_v1_storage(std::string const& path)
{
    using namespace sqlite_orm;
    return make_storage(path,
        make_index(
            "posts_listing", &Post::unlisted, &Post::created_at, &Post::id),
        make_table("posts",
            make_column("id", &Post::id, primary_key()),
            make_column("created_at", &Post::created_at),
            make_column("unlisted", &Post::unlisted, default_value(false)),
            make_column("parent_id", &Post::parent_id),
            make_column("title", &Post::title),
            make_column("author", &Post::author),
            make_column("body", &Post::body),
            foreign_key(&Post::parent_id).references(&Post::id)));
}

} // namespace

TEST_CASE("insert_post")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    auto post = make_post("7HcPq2mVa0Rn");
    post.title = "Euler's identity";
    post.author = "Euler";
    post.unlisted = true;
    db.with_storage(
        [&](auto& storage) { catalan::orm::insert_post(storage, post); });

    auto const stored = db.with_storage([](auto& storage) {
        return catalan::orm::get_post(storage, "7HcPq2mVa0Rn");
    });
    REQUIRE(stored);
    CHECK_EQ(stored->created_at, 1'761'275'700'000);
    CHECK_EQ(stored->title, "Euler's identity");
    CHECK_EQ(stored->author, "Euler");
    CHECK(stored->unlisted);
    CHECK_FALSE(stored->parent_id.has_value());
    CHECK_EQ(stored->body, "$$e^{i\\pi} + 1 = 0$$");
}

TEST_CASE("get_post")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    auto unlisted = make_post("7HcPq2mVa0Rn");
    unlisted.unlisted = true;
    db.with_storage(
        [&](auto& storage) { catalan::orm::insert_post(storage, unlisted); });

    db.with_storage([](auto& storage) {
        CHECK(catalan::orm::get_post(storage, "7HcPq2mVa0Rn"));
        CHECK_FALSE(catalan::orm::get_post(storage, "000000000000"));
    });
}

TEST_CASE("is_duplicate_id")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    db.with_storage([](auto& storage) {
        catalan::orm::insert_post(storage, make_post("7HcPq2mVa0Rn"));
    });

    try {
        db.with_storage([](auto& storage) {
            catalan::orm::insert_post(storage, make_post("7HcPq2mVa0Rn"));
        });
        FAIL("duplicate id was inserted");
    } catch (std::system_error const& e) {
        CHECK(catalan::orm::is_duplicate_id(e));
        CHECK_FALSE(catalan::orm::is_unknown_parent(e));
    }
}

TEST_CASE("is_unknown_parent")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    db.with_storage([](auto& storage) {
        catalan::orm::insert_post(storage, make_post("7HcPq2mVa0Rn"));
    });

    CHECK_NOTHROW(db.with_storage([](auto& storage) {
        catalan::orm::insert_post(storage,
            make_post("4fQx9ZtLb2Wk", 1'761'275'700'000, "7HcPq2mVa0Rn"));
    }));

    try {
        db.with_storage([](auto& storage) {
            catalan::orm::insert_post(storage,
                make_post("9aBcDeFgHiJk", 1'761'275'700'000, "000000000000"));
        });
        FAIL("post with an unknown parent was inserted");
    } catch (std::system_error const& e) {
        CHECK(catalan::orm::is_unknown_parent(e));
        CHECK_FALSE(catalan::orm::is_duplicate_id(e));
    }
}

TEST_CASE("list_posts")
{
    using catalan::orm::Order;
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    SUBCASE("empty table")
    {
        auto const page = db.with_storage([](auto& storage) {
            return catalan::orm::list_posts(
                storage, 20, Order::desc, std::nullopt);
        });
        CHECK(page.posts.empty());
        CHECK_FALSE(page.next.has_value());
    }

    SUBCASE("order and unlisted")
    {
        db.with_storage([](auto& storage) {
            catalan::orm::insert_post(storage, make_post(numbered_id(1), 100));
            catalan::orm::insert_post(storage, make_post(numbered_id(2), 300));
            catalan::orm::insert_post(storage, make_post(numbered_id(3), 200));
            auto hidden = make_post(numbered_id(4), 400);
            hidden.unlisted = true;
            catalan::orm::insert_post(storage, hidden);
        });

        auto const page = db.with_storage([](auto& storage) {
            return catalan::orm::list_posts(
                storage, 1, Order::desc, std::nullopt);
        });
        REQUIRE_EQ(page.posts.size(), 1);
        CHECK_EQ(page.posts[0].body_prefix, "$$e^{i\\pi} + 1 = 0$$");
        CHECK_EQ(page.posts[0].body_length,
            static_cast<std::int64_t>(page.posts[0].body_prefix.size()));

        CHECK_EQ(walk(db, 20, Order::desc),
            std::vector<std::string>{
                numbered_id(2), numbered_id(3), numbered_id(1) });
        CHECK_EQ(walk(db, 20, Order::asc),
            std::vector<std::string>{
                numbered_id(1), numbered_id(3), numbered_id(2) });
    }

    SUBCASE("exactly limit posts is one page")
    {
        db.with_storage([](auto& storage) {
            for (int i = 1; i <= 3; i++)
                catalan::orm::insert_post(
                    storage, make_post(numbered_id(i), i));
        });

        auto const page = db.with_storage([](auto& storage) {
            return catalan::orm::list_posts(
                storage, 3, Order::desc, std::nullopt);
        });
        CHECK_EQ(page.posts.size(), 3);
        CHECK_FALSE(page.next.has_value());
    }

    SUBCASE("limit + 1 posts is two pages")
    {
        db.with_storage([](auto& storage) {
            for (int i = 1; i <= 4; i++)
                catalan::orm::insert_post(
                    storage, make_post(numbered_id(i), i));
        });

        auto const first = db.with_storage([](auto& storage) {
            return catalan::orm::list_posts(
                storage, 3, Order::desc, std::nullopt);
        });
        REQUIRE(first.next);
        CHECK_EQ(first.next->id, first.posts.back().id);

        auto const second = db.with_storage([&](auto& storage) {
            return catalan::orm::list_posts(
                storage, 3, Order::desc, first.next);
        });
        CHECK_EQ(second.posts.size(), 1);
        CHECK_FALSE(second.next.has_value());
    }

    SUBCASE("shared timestamps are never skipped or repeated")
    {
        // 25 posts in 5 groups of the same millisecond, so page boundaries
        // land inside a group and only the id tie-break orders them
        db.with_storage([](auto& storage) {
            for (int i = 0; i < 25; i++)
                catalan::orm::insert_post(
                    storage, make_post(numbered_id(i), 1000 + i / 5));
        });

        for (auto const order : { Order::desc, Order::asc }) {
            for (std::size_t limit : { 1, 2, 3, 4, 7, 25, 100 }) {
                auto const ids = walk(db, limit, order);
                CHECK_EQ(ids.size(), 25);
                CHECK_EQ(
                    std::set<std::string>(ids.begin(), ids.end()).size(), 25);
            }
        }
    }
}

TEST_CASE("insert_post_once")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    std::string const key = "8e03978e-40d5-43e8-bc93-6894a57f9324";

    SUBCASE("the first use inserts, a reuse returns the key's post")
    {
        auto const first = db.with_storage([&](auto& storage) {
            return catalan::orm::insert_post_once(
                storage, make_post("7HcPq2mVa0Rn", 2000), key, 0);
        });
        CHECK_FALSE(first.has_value());

        auto other = make_post("4fQx9ZtLb2Wk", 3000);
        other.body = "something else";
        auto const second = db.with_storage([&](auto& storage) {
            return catalan::orm::insert_post_once(storage, other, key, 0);
        });
        REQUIRE(second);
        CHECK_EQ(second->id, "7HcPq2mVa0Rn");
        CHECK_EQ(second->body, "$$e^{i\\pi} + 1 = 0$$");

        CHECK_EQ(db.with_storage([](auto& storage) {
            return storage.template count<Post>();
        }),
            1);
    }

    SUBCASE("an expired key is removed and free to use again")
    {
        db.with_storage([&](auto& storage) {
            catalan::orm::insert_post_once(
                storage, make_post("7HcPq2mVa0Rn", 1000), key, 0);
        });

        auto const again = db.with_storage([&](auto& storage) {
            return catalan::orm::insert_post_once(
                storage, make_post("4fQx9ZtLb2Wk", 5000), key, 1001);
        });
        CHECK_FALSE(again.has_value());

        auto const used = db.with_storage([&](auto& storage) {
            return storage.template get_optional<catalan::orm::Idempotency_Key>(
                key);
        });
        REQUIRE(used);
        CHECK_EQ(used->post_id, "4fQx9ZtLb2Wk");
    }

    SUBCASE("a duplicate post id rolls back the key")
    {
        db.with_storage([](auto& storage) {
            catalan::orm::insert_post(storage, make_post("7HcPq2mVa0Rn"));
        });

        CHECK_THROWS_AS(db.with_storage([&](auto& storage) {
            return catalan::orm::insert_post_once(
                storage, make_post("7HcPq2mVa0Rn"), key, 0);
        }),
            std::system_error);

        CHECK_FALSE(db.with_storage([&](auto& storage) {
            return storage.template get_optional<catalan::orm::Idempotency_Key>(
                key);
        }));
    }
}

TEST_CASE("list_posts reads only a body prefix")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    auto post = make_post("7HcPq2mVa0Rn");
    post.body = std::string(10'000, 'x');
    db.with_storage(
        [&](auto& storage) { catalan::orm::insert_post(storage, post); });

    auto const page = db.with_storage([](auto& storage) {
        return catalan::orm::list_posts(
            storage, 20, catalan::orm::Order::desc, std::nullopt);
    });
    REQUIRE_EQ(page.posts.size(), 1);
    CHECK_EQ(page.posts[0].body_prefix.size(),
        static_cast<std::size_t>(catalan::orm::k_body_prefix_length));
    CHECK_EQ(page.posts[0].body_length, 10'000);
}

TEST_CASE("check_health")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    CHECK_NOTHROW(db.with_storage(
        [](auto& storage) { catalan::orm::check_health(storage); }));
}

TEST_CASE("migrate new database")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    db.with_storage([](auto& storage) {
        CHECK_EQ(storage.pragma.user_version(), catalan::orm::k_schema_version);
        CHECK_EQ(storage.pragma.journal_mode(), sqlite_orm::journal_mode::WAL);

        auto schema = storage.sync_schema_simulate();
        REQUIRE(schema.contains("posts_listing"));
        CHECK_EQ(schema.at("posts_listing"),
            sqlite_orm::sync_schema_result::already_in_sync);
    });
}

TEST_CASE("migrate up to date database")
{
    Temp_Database_Path temp{};
    {
        catalan::orm::Database db{ temp.string() };
        db.with_storage([](auto& storage) {
            catalan::orm::insert_post(storage, make_post("7HcPq2mVa0Rn"));
        });
    }

    catalan::orm::Database db{ temp.string() };
    CHECK_EQ(db.with_storage(
                 [](auto& storage) { return storage.template count<Post>(); }),
        1);
}

TEST_CASE("migrate 1 to 2")
{
    Temp_Database_Path temp{};
    {
        auto storage = make_v1_storage(temp.string());
        storage.sync_schema();
        storage.pragma.user_version(1);
        storage.replace(make_post("7HcPq2mVa0Rn"));
    }

    catalan::orm::Database db{ temp.string() };
    db.with_storage([](auto& storage) {
        CHECK_EQ(storage.pragma.user_version(), 2);
        CHECK(storage.table_exists("idempotency_keys"));
        CHECK_EQ(storage.template count<Post>(), 1);
    });
}

TEST_CASE("migrate refuses a step that isn't additive")
{
    struct Legacy_Post
    {
        std::string id{};
        std::string legacy{};
    };

    Temp_Database_Path temp{};
    {
        using namespace sqlite_orm;
        auto storage = make_storage(temp.string(),
            make_table("posts",
                make_column("id", &Legacy_Post::id, primary_key()),
                make_column("legacy", &Legacy_Post::legacy)));
        storage.sync_schema();
        storage.pragma.user_version(1);
        storage.replace(Legacy_Post{ "7HcPq2mVa0Rn", "kept" });
    }

    CHECK_THROWS_WITH_AS(catalan::orm::Database{ temp.string() },
        doctest::Contains("must only add"),
        std::runtime_error);

    using namespace sqlite_orm;
    auto storage = make_storage(temp.string(),
        make_table("posts",
            make_column("id", &Legacy_Post::id, primary_key()),
            make_column("legacy", &Legacy_Post::legacy)));
    CHECK_EQ(storage.pragma.user_version(), 1);
    CHECK_EQ(storage.get<Legacy_Post>("7HcPq2mVa0Rn").legacy, "kept");
}

TEST_CASE("migrate newer database")
{
    Temp_Database_Path temp{};
    {
        catalan::orm::Database db{ temp.string() };
        db.with_storage([](auto& storage) {
            storage.pragma.user_version(catalan::orm::k_schema_version + 1);
        });
    }

    CHECK_THROWS_AS(
        catalan::orm::Database{ temp.string() }, std::runtime_error);
}

TEST_CASE("migrate unversioned database")
{
    Temp_Database_Path temp{};
    {
        using namespace sqlite_orm;
        auto storage = make_storage(temp.string(),
            make_table("posts", make_column("id", &Post::id, primary_key())));
        storage.sync_schema();
    }

    CHECK_THROWS_AS(
        catalan::orm::Database{ temp.string() }, std::runtime_error);
}
