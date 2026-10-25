/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <doctest/doctest.h> // for ResultBuilder, CHECK, TEST_CASE

#include "fixture.hh"        // for Temp_Database_Path
#include <catalan/api.hh>    // for create_post, get_post, list_posts, ...
#include <catalan/manage.hh> // for Database, Cursor
#include <crow.h>            // for request, response, json
#include <cstddef>           // for size_t
#include <cstdint>           // for int64_t
#include <string>            // for basic_string, string
#include <string_view>       // for string_view
#include <unordered_set>     // for unordered_set

namespace {

using catalan::test::Temp_Database_Path;

crow::request make_request(std::string body = "",
    std::string content_type = "application/json")
{
    crow::request req{};
    req.body = std::move(body);
    if (not content_type.empty())
        req.headers.emplace("Content-Type", std::move(content_type));
    return req;
}

/**
 * @brief A GET request with a query string, parsed the way Crow parses it
 *
 *  Example:
 *
 *    make_query("?limit=2&order=asc");
 */
crow::request make_query(std::string const& query)
{
    crow::request req{};
    req.url_params = crow::query_string{ "/v1/posts" + query };
    return req;
}

/**
 * @brief Create a post through the handler, return its id
 */
std::string create(catalan::orm::Database& db, std::string const& body)
{
    auto res = catalan::api::create_post(db, make_request(body));
    REQUIRE_EQ(res.code, 201);
    return crow::json::load(res.body)["id"].s();
}

} // namespace

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

TEST_CASE("to_iso8601")
{
    CHECK_EQ(catalan::api::to_iso8601(1'761'275'700'123),
        "2025-10-24T03:15:00.123Z");
    CHECK_EQ(catalan::api::to_iso8601(0), "1970-01-01T00:00:00.000Z");
}

TEST_CASE("make_excerpt")
{
    using catalan::api::make_excerpt;

    CHECK_EQ(make_excerpt("short", 5), "short");
    std::string const spaced = "  # Title\n\n  body\ttext  ";
    CHECK_EQ(make_excerpt(spaced, static_cast<std::int64_t>(spaced.size())),
        "# Title body text");
    CHECK_EQ(make_excerpt("", 0), "");

    // exactly 100 characters is not cut, 101 is
    CHECK_EQ(make_excerpt(std::string(100, 'a'), 100), std::string(100, 'a'));
    CHECK_EQ(
        make_excerpt(std::string(101, 'a'), 101), std::string(100, 'a') + "…");

    // a 3-byte character counts once and is never split
    std::string sums{};
    for (int i = 0; i < 120; i++)
        sums += "∑";
    auto const excerpt = make_excerpt(sums, 120);
    CHECK_EQ(excerpt.size(), 100 * 3 + std::string{ "…" }.size());

    // a prefix shorter than the body is cut even when it fits
    CHECK_EQ(
        make_excerpt("start of a long body", 5000), "start of a long body…");
}

TEST_CASE("html_escape")
{
    CHECK_EQ(catalan::api::html_escape(R"(<b>"x" & 'y'</b>)"),
        "&lt;b&gt;&quot;x&quot; &amp; &#39;y&#39;&lt;/b&gt;");
    CHECK_EQ(catalan::api::html_escape("$$e^{i\\pi}$$"), "$$e^{i\\pi}$$");
}

TEST_CASE("render_meta")
{
    SUBCASE("defaults without a post")
    {
        auto const meta =
            catalan::api::render_meta(std::nullopt, "https://catalan.ly");
        CHECK(meta.find("<title>catalan</title>") != std::string::npos);
        CHECK(meta.find("og:url") == std::string::npos);
    }

    SUBCASE("a post's title, author, excerpt, and url")
    {
        catalan::orm::Post post{};
        post.id = "7HcPq2mVa0Rn";
        post.title = "Euler's identity";
        post.author = "Euler";
        post.body = "$$e^{i\\pi} + 1 = 0$$";
        auto const meta = catalan::api::render_meta(post, "https://catalan.ly");
        CHECK(meta.find("<title>Euler&#39;s identity · catalan</title>") !=
              std::string::npos);
        CHECK(meta.find(R"(content="by Euler · $$e^{i\pi} + 1 = 0$$")") !=
              std::string::npos);
        CHECK(meta.find(R"(content="https://catalan.ly/7HcPq2mVa0Rn")") !=
              std::string::npos);
        CHECK(meta.find("noindex") == std::string::npos);
    }

    SUBCASE("markup in a post can't escape its attribute")
    {
        catalan::orm::Post post{};
        post.id = "7HcPq2mVa0Rn";
        post.title = R"("><script>alert(1)</script>)";
        post.body = R"(</title><script>alert(2)</script>)";
        auto const meta = catalan::api::render_meta(post, "https://catalan.ly");
        CHECK(meta.find("<script>") == std::string::npos);
        CHECK(meta.find("&lt;script&gt;") != std::string::npos);
    }

    SUBCASE("unlisted posts aren't indexed")
    {
        catalan::orm::Post post{};
        post.id = "7HcPq2mVa0Rn";
        post.body = "x";
        post.unlisted = true;
        auto const meta = catalan::api::render_meta(post, "https://catalan.ly");
        CHECK(meta.find(R"(<meta name="robots" content="noindex">)") !=
              std::string::npos);
        CHECK(meta.find("<title>Untitled post · catalan</title>") !=
              std::string::npos);
    }
}

TEST_CASE("encode_cursor and decode_cursor")
{
    catalan::orm::Cursor const cursor{ 1'761'275'700'123, "7HcPq2mVa0Rn" };
    auto const decoded =
        catalan::api::decode_cursor(catalan::api::encode_cursor(cursor));
    REQUIRE(decoded);
    CHECK_EQ(decoded->created_at, cursor.created_at);
    CHECK_EQ(decoded->id, cursor.id);

    CHECK_FALSE(catalan::api::decode_cursor(""));
    CHECK_FALSE(catalan::api::decode_cursor("not a cursor!"));
    CHECK_FALSE(catalan::api::decode_cursor(
        crow::utility::base64encode_urlsafe("123", 3)));
    CHECK_FALSE(catalan::api::decode_cursor(
        crow::utility::base64encode_urlsafe("-1.7HcPq2mVa0Rn", 15)));
    CHECK_FALSE(catalan::api::decode_cursor(
        crow::utility::base64encode_urlsafe("123.short", 9)));
}

TEST_CASE("parse_idempotency_key")
{
    using catalan::api::parse_idempotency_key;
    std::string const uuid = "8e03978e-40d5-43e8-bc93-6894a57f9324";

    CHECK_EQ(parse_idempotency_key("\"" + uuid + "\""), uuid);
    CHECK_EQ(parse_idempotency_key(uuid), uuid);

    CHECK_FALSE(parse_idempotency_key("\"abc\""));
    CHECK_FALSE(parse_idempotency_key("two words, 16+ chars"));
    CHECK_FALSE(parse_idempotency_key("\"" + uuid));
    CHECK_FALSE(parse_idempotency_key(std::string(256, 'a')));
    CHECK(parse_idempotency_key(std::string(255, 'a')));
}

TEST_CASE("validate_new_post")
{
    auto const errors = [](std::string const& body) {
        return catalan::api::validate_new_post(crow::json::load(body));
    };

    CHECK(errors(R"({ "body": "x" })").empty());
    CHECK(errors(R"({ "body": "x", "title": "t", "author": "a",
                      "unlisted": true, "parent_id": "7HcPq2mVa0Rn" })")
            .empty());

    CHECK_EQ(errors(R"({})").size(), 1);
    CHECK_EQ(errors(R"({ "body": "" })")[0].message, "must not be empty");
    CHECK_EQ(errors(R"({ "body": 5 })")[0].message, "must be a string");
    CHECK_EQ(
        errors(R"({ "body": "x", "unlisted": "yes" })")[0].field, "unlisted");
    CHECK_EQ(errors(R"({ "body": "x", "parent_id": "nope" })")[0].field,
        "parent_id");
    CHECK_EQ(
        errors(R"({ "body": "x", "extra": 1 })")[0].message, "is not allowed");
    CHECK_EQ(errors(R"({ "body": "", "title": 1, "extra": 1 })").size(), 3);

    // 64 characters of a 3-byte character is 192 bytes, still within limit
    std::string author{};
    for (int i = 0; i < 64; i++)
        author += "∑";
    CHECK(errors(R"({ "body": "x", "author": ")" + author + R"(" })").empty());
    CHECK_EQ(
        errors(R"({ "body": "x", "author": ")" + author + R"(∑" })").size(), 1);
}

TEST_CASE("problem")
{
    auto res = catalan::api::problem(422,
        "Validation failed",
        "",
        {
            { "body", "must not be empty" }
    });
    CHECK_EQ(res.code, 422);
    CHECK_EQ(res.get_header_value("Content-Type"), "application/problem+json");

    auto const json = crow::json::load(res.body);
    CHECK_EQ(std::string{ json["type"].s() }, "/problems/validation");
    CHECK_EQ(json["status"].i(), 422);
    CHECK_FALSE(json.has("detail"));
    CHECK_EQ(std::string{ json["errors"][0]["field"].s() }, "body");
}

TEST_CASE("create_post")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    SUBCASE("201 with Location and the post")
    {
        auto res = catalan::api::create_post(db,
            make_request(
                R"({ "body": "$$e^{i\\pi} + 1 = 0$$", "author": "Euler" })"));
        CHECK_EQ(res.code, 201);

        auto const json = crow::json::load(res.body);
        auto const id = std::string{ json["id"].s() };
        CHECK(catalan::api::is_post_id(id));
        CHECK_EQ(res.get_header_value("Location"), "/v1/posts/" + id);
        CHECK_EQ(std::string{ json["author"].s() }, "Euler");
        CHECK_EQ(json["title"].t(), crow::json::type::Null);
        CHECK_EQ(json["unlisted"].t(), crow::json::type::False);
    }

    SUBCASE("415 without application/json")
    {
        CHECK_EQ(catalan::api::create_post(
                     db, make_request(R"({ "body": "x" })", "text/plain"))
                     .code,
            415);
        CHECK_EQ(catalan::api::create_post(
                     db, make_request(R"({ "body": "x" })", ""))
                     .code,
            415);
    }

    SUBCASE("400 for a body that isn't a JSON object")
    {
        CHECK_EQ(catalan::api::create_post(db, make_request("{ not json")).code,
            400);
        CHECK_EQ(
            catalan::api::create_post(db, make_request("[1, 2]")).code, 400);
    }

    SUBCASE("422 for invalid fields")
    {
        CHECK_EQ(
            catalan::api::create_post(db, make_request(R"({ "body": "" })"))
                .code,
            422);
    }

    SUBCASE("422 for a parent that doesn't exist")
    {
        auto res = catalan::api::create_post(db,
            make_request(R"({ "body": "x", "parent_id": "000000000000" })"));
        CHECK_EQ(res.code, 422);
        CHECK_EQ(
            std::string{ crow::json::load(res.body)["errors"][0]["field"].s() },
            "parent_id");
    }

    SUBCASE("201 for an edit of an existing post")
    {
        auto const parent = create(db, R"({ "body": "x" })");
        auto res = catalan::api::create_post(db,
            make_request(
                R"({ "body": "y", "parent_id": ")" + parent + R"(" })"));
        CHECK_EQ(res.code, 201);
        CHECK_EQ(
            std::string{ crow::json::load(res.body)["parent_id"].s() }, parent);
    }
}

TEST_CASE("create_post with Idempotency-Key")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    std::string const key = "\"8e03978e-40d5-43e8-bc93-6894a57f9324\"";

    auto const keyed = [&](std::string body, std::string header) {
        auto req = make_request(std::move(body));
        req.headers.emplace("Idempotency-Key", std::move(header));
        return catalan::api::create_post(db, req);
    };

    SUBCASE("a retry replays the original 201")
    {
        auto first = keyed(R"({ "body": "x", "title": "t" })", key);
        auto retry = keyed(R"({ "body": "x", "title": "t" })", key);

        CHECK_EQ(first.code, 201);
        CHECK_EQ(retry.code, 201);
        CHECK(first.get_header_value("Idempotent-Replayed").empty());
        CHECK_EQ(retry.get_header_value("Idempotent-Replayed"), "true");
        CHECK_EQ(retry.get_header_value("Location"),
            first.get_header_value("Location"));

        auto const original = crow::json::load(first.body);
        auto const replayed = crow::json::load(retry.body);
        CHECK_EQ(std::string{ replayed["id"].s() },
            std::string{ original["id"].s() });
        CHECK_EQ(std::string{ replayed["created_at"].s() },
            std::string{ original["created_at"].s() });
    }

    SUBCASE("the same post in different JSON is still a replay")
    {
        auto first = keyed(R"({"title":"t","body":"x"})", key);
        auto retry = keyed(R"({ "body" : "x",   "title" : "t" })",
            "8e03978e-40d5-43e8-bc93-6894a57f9324");
        CHECK_EQ(retry.code, 201);
        CHECK_EQ(retry.get_header_value("Location"),
            first.get_header_value("Location"));
    }

    SUBCASE("a different post under the same key is a 422")
    {
        CHECK_EQ(keyed(R"({ "body": "x" })", key).code, 201);
        auto res = keyed(R"({ "body": "y" })", key);
        CHECK_EQ(res.code, 422);
        CHECK_EQ(std::string{ crow::json::load(res.body)["type"].s() },
            "/problems/idempotency-key-reused");
    }

    SUBCASE("a malformed key is a 400")
    {
        CHECK_EQ(keyed(R"({ "body": "x" })", "\"short\"").code, 400);
    }

    SUBCASE("without a key, a retry creates a second post")
    {
        auto first =
            catalan::api::create_post(db, make_request(R"({ "body": "x" })"));
        auto again =
            catalan::api::create_post(db, make_request(R"({ "body": "x" })"));
        CHECK_NE(first.get_header_value("Location"),
            again.get_header_value("Location"));
    }
}

TEST_CASE("get_post")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    auto const id = create(db, R"({ "body": "x", "unlisted": true })");

    SUBCASE("200 with ETag and Cache-Control, unlisted included")
    {
        auto res = catalan::api::get_post(db, crow::request{}, id);
        CHECK_EQ(res.code, 200);
        CHECK_EQ(res.get_header_value("ETag"), "\"" + id + "\"");
        CHECK_EQ(
            res.get_header_value("Cache-Control"), "public, max-age=86400");
        CHECK_EQ(std::string{ crow::json::load(res.body)["id"].s() }, id);
    }

    SUBCASE("304 for a matching If-None-Match")
    {
        crow::request req{};
        req.headers.emplace("If-None-Match", "W/\"other\", \"" + id + "\"");
        auto res = catalan::api::get_post(db, req, id);
        CHECK_EQ(res.code, 304);
        CHECK(res.body.empty());
    }

    SUBCASE("404, not 304, for a removed post")
    {
        db.with_storage([&](auto& storage) {
            storage.template remove<catalan::orm::Post>(id);
        });
        crow::request req{};
        req.headers.emplace("If-None-Match", "\"" + id + "\"");
        CHECK_EQ(catalan::api::get_post(db, req, id).code, 404);
    }

    SUBCASE("404 for unknown and malformed ids")
    {
        CHECK_EQ(
            catalan::api::get_post(db, crow::request{}, "000000000000").code,
            404);
        CHECK_EQ(
            catalan::api::get_post(db, crow::request{}, "../etc").code, 404);
    }
}

TEST_CASE("get_post_raw")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    auto const id = create(db, R"({ "body": "$$e^{i\\pi} + 1 = 0$$" })");

    auto res = catalan::api::get_post_raw(db, crow::request{}, id);
    CHECK_EQ(res.code, 200);
    CHECK_EQ(res.body, "$$e^{i\\pi} + 1 = 0$$");
    CHECK_EQ(res.get_header_value("Content-Type"), "text/plain; charset=utf-8");
}

TEST_CASE("list_posts handler")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    for (int i = 0; i < 5; i++)
        create(db, R"({ "body": "x" })");
    create(db, R"({ "body": "hidden", "unlisted": true })");

    SUBCASE("pages follow next_cursor to the end")
    {
        std::size_t seen = 0;
        std::string query = "?limit=2";
        for (;;) {
            auto res = catalan::api::list_posts(db, make_query(query));
            REQUIRE_EQ(res.code, 200);
            auto const json = crow::json::load(res.body);
            seen += json["posts"].size();
            if (json["next_cursor"].t() == crow::json::type::Null)
                break;
            query = "?limit=2&cursor=" + std::string{ json["next_cursor"].s() };
        }
        CHECK_EQ(seen, 5);
    }

    SUBCASE("400 for bad query parameters")
    {
        for (std::string_view const query : { "?limit=0",
                 "?limit=101",
                 "?limit=abc",
                 "?limit=-1",
                 "?order=sideways",
                 "?cursor=garbage!" })
            CHECK_EQ(
                catalan::api::list_posts(db, make_query(std::string{ query }))
                    .code,
                400);
    }
}

TEST_CASE("get_post_meta")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };
    auto const id = create(db, R"({ "body": "x", "title": "Hello" })");

    auto known = catalan::api::get_post_meta(db, id, "https://catalan.ly");
    CHECK_EQ(known.code, 200);
    CHECK_EQ(
        known.get_header_value("Content-Type"), "text/html; charset=utf-8");
    CHECK(
        known.body.find("<title>Hello · catalan</title>") != std::string::npos);

    for (auto const& missing :
        { std::string{ "000000000000" }, std::string{ "../x" } }) {
        auto res =
            catalan::api::get_post_meta(db, missing, "https://catalan.ly");
        CHECK_EQ(res.code, 200);
        CHECK(res.body.find("<title>catalan</title>") != std::string::npos);
    }
}

TEST_CASE("get_health")
{
    Temp_Database_Path temp{};
    catalan::orm::Database db{ temp.string() };

    auto res = catalan::api::get_health(db);
    CHECK_EQ(res.code, 200);
    CHECK_EQ(res.body, "ok");
}
