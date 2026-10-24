/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <catalan/api.hh>

#include <algorithm>         // for all_of, count_if, find, transform
#include <catalan/manage.hh> // for Database, Post, Post_Summary, Cursor
#include <charconv>          // for from_chars
#include <chrono>            // for milliseconds, sys_time, system_clock
#include <crow.h>            // for request, response, json, CROW_LOG_ERROR
#include <cstddef>           // for size_t
#include <cstdint>           // for int64_t
#include <exception>         // for exception
#include <format>            // for format
#include <iterator>          // for back_inserter
#include <optional>          // for optional, nullopt
#include <string>            // for basic_string, string, to_string
#include <string_view>       // for basic_string_view, string_view
#include <system_error>      // for errc, system_error
#include <utility>           // for move
#include <vector>            // for vector

/****************************************************************************
 * API
 *
 * Handler and helper bodies, see api.hh. Handlers catch storage failures,
 * log the reason, and answer with a generic Problem Details response. An
 * exception's message never reaches the client.
 ****************************************************************************/

namespace catalan::api {

namespace {

inline constexpr std::string_view k_json_type = "application/json";
inline constexpr std::string_view k_problem_type = "application/problem+json";
inline constexpr std::string_view k_cache_immutable =
    "public, max-age=31536000, immutable";

/**
 * @brief Length in Unicode code points, the unit of JSON Schema's maxLength
 *
 * Counts every byte that isn't a UTF-8 continuation byte (10xxxxxx).
 *
 *  Examples:
 *
 *    utf8_length("abc");  // 3
 *    utf8_length("∑");    // 1, three bytes
 */
std::size_t utf8_length(std::string_view text)
{
    return static_cast<std::size_t>(std::count_if(text.begin(),
        text.end(),
        [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

/**
 * @brief The relative "type" URI for a Problem Details status
 */
std::string_view problem_slug(int status)
{
    switch (status) {
        case 400:
            return "/problems/malformed-request";
        case 404:
            return "/problems/not-found";
        case 415:
            return "/problems/unsupported-media-type";
        case 422:
            return "/problems/validation";
        case 503:
            return "/problems/unavailable";
        default:
            return "/problems/internal";
    }
}

/**
 * @brief Set a JSON field to an optional's value, or null
 */
void set_nullable(crow::json::wvalue& json,
    std::string const& key,
    std::optional<std::string> const& value)
{
    if (value)
        json[key] = *value;
    else
        json[key] = nullptr;
}

/**
 * @brief Quote a post id as its ETag
 *
 *  Example:
 *
 *    etag_for("7HcPq2mVa0Rn"); // "\"7HcPq2mVa0Rn\""
 */
std::string etag_for(std::string const& id)
{
    return "\"" + id + "\"";
}

/**
 * @brief Check an If-None-Match header against an ETag
 *
 * The header is a comma-separated list, or "*" for any version. A weak
 * W/ prefix compares the same, posts never change.
 *
 *  Examples:
 *
 *    if_none_match("\"7HcPq2mVa0Rn\"", etag);           // true
 *    if_none_match("\"x\", W/\"7HcPq2mVa0Rn\"", etag);  // true
 *    if_none_match("*", etag);                          // true
 *    if_none_match("\"4fQx9ZtLb2Wk\"", etag);           // false
 */
bool if_none_match(std::string_view header, std::string_view etag)
{
    while (not header.empty()) {
        auto const comma = header.find(',');
        auto tag = header.substr(0, comma);
        header = comma == std::string_view::npos ? std::string_view{}
                                                 : header.substr(comma + 1);

        auto const first = tag.find_first_not_of(" \t");
        if (first == std::string_view::npos)
            continue;
        tag = tag.substr(first, tag.find_last_not_of(" \t") - first + 1);
        if (tag.starts_with("W/"))
            tag.remove_prefix(2);

        if (tag == "*" or tag == etag)
            return true;
    }
    return false;
}

/**
 * @brief Parse a whole string as a non-negative integer, nothing else
 *
 *  Examples:
 *
 *    parse_unsigned("20");   // 20
 *    parse_unsigned("20a");  // nullopt
 *    parse_unsigned("-1");   // nullopt
 */
template<typename T>
std::optional<T> parse_unsigned(std::string_view text)
{
    T value{};
    if (text.empty() or text.front() == '-' or text.front() == '+')
        return std::nullopt;
    auto const [end, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} or end != text.data() + text.size())
        return std::nullopt;
    return value;
}

/**
 * @brief Log a storage failure and answer with a generic 500
 */
crow::response internal_error(std::string_view where,
    std::exception const& error)
{
    CROW_LOG_ERROR << where << ": " << error.what();
    return problem(500, "Internal server error");
}

/**
 * @brief Look up a post and answer for GET /v1/posts/{id} and its /raw
 *
 * The 404, 304, and cache headers are the same for both, only the 200 body
 * differs. A malformed id and a matching If-None-Match never touch the
 * database.
 */
template<typename Render>
crow::response respond_with_post(orm::Database& db,
    crow::request const& req,
    std::string const& id,
    Render render)
{
    if (not is_post_id(id))
        return problem(404, "Post not found");

    auto const etag = etag_for(id);
    auto const& match = req.get_header_value("If-None-Match");
    if (not match.empty() and if_none_match(match, etag)) {
        crow::response res{ 304 };
        res.set_header("ETag", etag);
        res.set_header("Cache-Control", std::string{ k_cache_immutable });
        return res;
    }

    std::optional<orm::Post> post{};
    try {
        post = db.with_storage(
            [&](auto& storage) { return orm::get_post(storage, id); });
    } catch (std::exception const& e) {
        return internal_error("get_post", e);
    }

    if (not post)
        return problem(404, "Post not found");

    crow::response res = render(*post);
    res.set_header("ETag", etag);
    res.set_header("Cache-Control", std::string{ k_cache_immutable });
    return res;
}

/**
 * @brief Check a keyed retry asks for the same post the key created
 *
 * Posts are immutable, so the stored post is the original request. The
 * fields are compared rather than the raw JSON, so whitespace and key order
 * don't matter.
 */
bool same_post(orm::Post const& stored, orm::Post const& requested)
{
    return stored.title == requested.title and
           stored.author == requested.author and
           stored.body == requested.body and
           stored.unlisted == requested.unlisted and
           stored.parent_id == requested.parent_id;
}

/**
 * @brief The 201 Created response for a post
 */
crow::response created(orm::Post const& post)
{
    crow::response res{ 201, to_json(post).dump() };
    res.set_header("Content-Type", std::string{ k_json_type });
    res.set_header("Location", "/v1/posts/" + post.id);
    return res;
}

} // namespace

/****************************************************************************
 * Helpers
 ****************************************************************************/

std::int64_t now_ms()
{
    auto const now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch())
        .count();
}

std::string to_iso8601(std::int64_t milliseconds)
{
    // millisecond precision, so %T prints exactly 3 fractional digits
    std::chrono::sys_time<std::chrono::milliseconds> const time{
        std::chrono::milliseconds{ milliseconds }
    };
    return std::format("{:%FT%T}Z", time);
}

std::string encode_cursor(orm::Cursor const& cursor)
{
    auto const plain = std::to_string(cursor.created_at) + "." + cursor.id;
    auto encoded = crow::utility::base64encode_urlsafe(plain, plain.size());
    // padding carries no information, and a bare = is fragile in a query
    encoded.erase(encoded.find_last_not_of('=') + 1);
    return encoded;
}

std::optional<orm::Cursor> decode_cursor(std::string_view cursor)
{
    // crow's decoder maps unknown characters to 0 instead of failing
    auto const is_base64url = [](char c) {
        return (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z') or
               (c >= '0' and c <= '9') or c == '-' or c == '_' or c == '=';
    };
    if (cursor.empty() or
        not std::all_of(cursor.begin(), cursor.end(), is_base64url))
        return std::nullopt;

    auto const plain =
        crow::utility::base64decode(std::string{ cursor }, cursor.size());

    auto const dot = plain.find('.');
    if (dot == std::string::npos)
        return std::nullopt;

    auto const created_at =
        parse_unsigned<std::int64_t>(std::string_view{ plain }.substr(0, dot));
    auto const id = plain.substr(dot + 1);
    if (not created_at or not is_post_id(id))
        return std::nullopt;

    return orm::Cursor{ *created_at, id };
}

std::optional<std::string> parse_idempotency_key(std::string_view header)
{
    if (header.size() >= 2 and header.front() == '"' and header.back() == '"') {
        header.remove_prefix(1);
        header.remove_suffix(1);
    }

    auto const visible = [](char c) { return c >= 0x21 and c <= 0x7E; };
    if (header.size() < k_idempotency_key_min_length or
        header.size() > k_idempotency_key_max_length or
        not std::all_of(header.begin(), header.end(), visible) or
        header.find('"') != std::string_view::npos)
        return std::nullopt;

    return std::string{ header };
}

std::vector<Field_Error> validate_new_post(crow::json::rvalue const& body)
{
    using crow::json::type;
    std::vector<Field_Error> errors{};

    auto const allowed = { "body", "title", "author", "unlisted", "parent_id" };
    for (auto const& key : body.keys()) {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
            errors.push_back({ key, "is not allowed" });
    }

    if (not body.has("body")) {
        errors.push_back({ "body", "is required" });
    } else if (body["body"].t() != type::String) {
        errors.push_back({ "body", "must be a string" });
    } else {
        auto const length = utf8_length(std::string{ body["body"].s() });
        if (length == 0)
            errors.push_back({ "body", "must not be empty" });
        else if (length > k_body_max_length)
            errors.push_back({ "body",
                std::format(
                    "must be at most {} characters", k_body_max_length) });
    }

    for (auto const& [field, max_length] : {
             std::pair{ "title",  k_title_max_length  },
             std::pair{ "author", k_author_max_length }
    }) {
        if (not body.has(field))
            continue;
        if (body[field].t() != type::String)
            errors.push_back({ field, "must be a string" });
        else if (utf8_length(std::string{ body[field].s() }) > max_length)
            errors.push_back({ field,
                std::format("must be at most {} characters", max_length) });
    }

    if (body.has("unlisted") and body["unlisted"].t() != type::True and
        body["unlisted"].t() != type::False)
        errors.push_back({ "unlisted", "must be a boolean" });

    if (body.has("parent_id")) {
        if (body["parent_id"].t() != type::String)
            errors.push_back({ "parent_id", "must be a string" });
        else if (not is_post_id(std::string{ body["parent_id"].s() }))
            errors.push_back({ "parent_id", "is not a valid post id" });
    }

    return errors;
}

crow::response problem(int status,
    std::string_view title,
    std::string_view detail,
    std::vector<Field_Error> const& errors,
    std::string_view type)
{
    crow::json::wvalue json{};
    json["type"] = std::string{ type.empty() ? problem_slug(status) : type };
    json["title"] = std::string{ title };
    json["status"] = status;
    if (not detail.empty())
        json["detail"] = std::string{ detail };
    if (not errors.empty()) {
        crow::json::wvalue::list list{};
        for (auto const& error : errors) {
            crow::json::wvalue entry{};
            entry["field"] = error.field;
            entry["message"] = error.message;
            list.push_back(std::move(entry));
        }
        json["errors"] = std::move(list);
    }

    crow::response res{ status, json.dump() };
    res.set_header("Content-Type", std::string{ k_problem_type });
    return res;
}

crow::json::wvalue to_json(orm::Post const& post)
{
    crow::json::wvalue json{};
    json["id"] = post.id;
    set_nullable(json, "title", post.title);
    set_nullable(json, "author", post.author);
    json["body"] = post.body;
    json["unlisted"] = post.unlisted;
    set_nullable(json, "parent_id", post.parent_id);
    json["created_at"] = to_iso8601(post.created_at);
    return json;
}

crow::json::wvalue to_json(orm::Post_Summary const& post)
{
    crow::json::wvalue json{};
    json["id"] = post.id;
    set_nullable(json, "title", post.title);
    set_nullable(json, "author", post.author);
    set_nullable(json, "parent_id", post.parent_id);
    json["created_at"] = to_iso8601(post.created_at);
    return json;
}

/****************************************************************************
 * Handlers
 ****************************************************************************/

crow::response create_post(orm::Database& db, crow::request const& req)
{
    if (not req.get_header_value("Content-Type").starts_with(k_json_type))
        return problem(415,
            "Unsupported media type",
            "Content-Type must be application/json.");

    std::optional<std::string> key{};
    if (auto const& header = req.get_header_value("Idempotency-Key");
        not header.empty()) {
        key = parse_idempotency_key(header);
        if (not key)
            return problem(400,
                "Malformed request",
                std::format("Idempotency-Key must be {} to {} visible ASCII "
                            "characters.",
                    k_idempotency_key_min_length,
                    k_idempotency_key_max_length));
    }

    auto const body = crow::json::load(req.body);
    if (not body or body.t() != crow::json::type::Object)
        return problem(
            400, "Malformed request", "Request body is not a JSON object.");

    if (auto const errors = validate_new_post(body); not errors.empty())
        return problem(422,
            "Validation failed",
            std::format("The request body has {} invalid field{}.",
                errors.size(),
                errors.size() == 1 ? "" : "s"),
            errors);

    // built outside with_storage, the lock only covers the database
    orm::Post post{};
    post.created_at = now_ms();
    post.body = body["body"].s();
    if (body.has("title"))
        post.title = body["title"].s();
    if (body.has("author"))
        post.author = body["author"].s();
    if (body.has("unlisted"))
        post.unlisted = body["unlisted"].b();
    if (body.has("parent_id"))
        post.parent_id = body["parent_id"].s();

    for (int attempt = 0; attempt < k_insert_attempts; attempt++) {
        post.id = make_post_id();

        std::optional<orm::Post> existing{};
        try {
            existing =
                db.with_storage([&](auto& storage) -> std::optional<orm::Post> {
                    if (not key) {
                        orm::insert_post(storage, post);
                        return std::nullopt;
                    }
                    return orm::insert_post_once(storage,
                        post,
                        *key,
                        post.created_at - k_idempotency_ttl_ms);
                });
        } catch (std::system_error const& e) {
            if (orm::is_duplicate_id(e))
                continue;
            if (orm::is_unknown_parent(e))
                return problem(422,
                    "Validation failed",
                    "The request body has 1 invalid field.",
                    {
                        { "parent_id",
                         "no post with id " + *post.parent_id }
                });
            return internal_error("create_post", e);
        } catch (std::exception const& e) {
            return internal_error("create_post", e);
        }

        if (not existing)
            return created(post);

        if (not same_post(*existing, post))
            return problem(422,
                "Idempotency-Key reused",
                "This Idempotency-Key was already used for a different post.",
                {},
                "/problems/idempotency-key-reused");

        auto res = created(*existing);
        res.set_header("Idempotent-Replayed", "true");
        return res;
    }

    CROW_LOG_ERROR << "create_post: " << k_insert_attempts
                   << " duplicate ids in a row";
    return problem(500, "Internal server error");
}

crow::response list_posts(orm::Database& db, crow::request const& req)
{
    std::size_t limit = k_list_limit_default;
    if (char const* value = req.url_params.get("limit")) {
        auto const parsed = parse_unsigned<std::size_t>(value);
        if (not parsed or *parsed < 1 or *parsed > k_list_limit_max)
            return problem(400,
                "Malformed request",
                std::format("limit must be an integer from 1 to {}.",
                    k_list_limit_max));
        limit = *parsed;
    }

    auto order = orm::Order::desc;
    if (char const* value = req.url_params.get("order")) {
        std::string_view const text{ value };
        if (text == "asc")
            order = orm::Order::asc;
        else if (text != "desc")
            return problem(
                400, "Malformed request", "order must be desc or asc.");
    }

    std::optional<orm::Cursor> after{};
    if (char const* value = req.url_params.get("cursor")) {
        after = decode_cursor(value);
        if (not after)
            return problem(400, "Malformed request", "cursor is not valid.");
    }

    orm::Page page{};
    try {
        page = db.with_storage([&](auto& storage) {
            return orm::list_posts(storage, limit, order, after);
        });
    } catch (std::exception const& e) {
        return internal_error("list_posts", e);
    }

    crow::json::wvalue::list posts{};
    std::transform(page.posts.begin(),
        page.posts.end(),
        std::back_inserter(posts),
        [](orm::Post_Summary const& post) { return to_json(post); });

    crow::json::wvalue json{};
    json["posts"] = std::move(posts);
    if (page.next)
        json["next_cursor"] = encode_cursor(*page.next);
    else
        json["next_cursor"] = nullptr;

    crow::response res{ 200, json.dump() };
    res.set_header("Content-Type", std::string{ k_json_type });
    return res;
}

crow::response get_post(orm::Database& db,
    crow::request const& req,
    std::string const& id)
{
    return respond_with_post(db, req, id, [](orm::Post const& post) {
        crow::response res{ 200, to_json(post).dump() };
        res.set_header("Content-Type", std::string{ k_json_type });
        return res;
    });
}

crow::response get_post_raw(orm::Database& db,
    crow::request const& req,
    std::string const& id)
{
    return respond_with_post(db, req, id, [](orm::Post const& post) {
        crow::response res{ 200, post.body };
        res.set_header("Content-Type", "text/plain; charset=utf-8");
        return res;
    });
}

crow::response get_health(orm::Database& db)
{
    try {
        db.with_storage([](auto& storage) { orm::check_health(storage); });
    } catch (std::exception const& e) {
        CROW_LOG_ERROR << "get_health: " << e.what();
        return problem(
            503, "Service unavailable", "The database is unreachable.");
    }

    crow::response res{ 200, "ok" };
    res.set_header("Content-Type", "text/plain; charset=utf-8");
    return res;
}

} // namespace catalan::api
