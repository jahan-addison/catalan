/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <algorithm>         // for all_of, generate
#include <catalan/manage.hh> // for Database, Post, Post_Summary, Cursor
#include <crow.h>            // for request, response, json
#include <cstddef>           // for size_t
#include <cstdint>           // for int64_t
#include <optional>          // for optional
#include <random>            // for random_device, uniform_int_distribution
#include <string>            // for basic_string, string
#include <string_view>       // for basic_string_view, string_view
#include <vector>            // for vector

/****************************************************************************
 * API
 *
 * Request and response logic for the HTTP routes registered in main.cc.
 * One handler per endpoint, named after its operationId in openapi.yaml,
 * plus the helpers they share. Handlers are plain functions, so the tests
 * call them with a crow::request and no running server.
 *
 *  Example:
 *
 *  POST /v1/posts
 *  { "body": "$$e^{i\\pi} + 1 = 0$$" }
 *
 *  Becomes:
 *
 *  201 Created
 *  Location: /v1/posts/7HcPq2mVa0Rn
 *
 ****************************************************************************/

namespace catalan::api {

/**
 * @brief Base62 alphabet for post ids
 *
 * Digits, lowercase, then uppercase. No symbols, so an id is always safe in
 * a URL and easy to copy.
 */
inline constexpr std::string_view k_id_alphabet =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/**
 * @brief Post id length
 *
 * 62^12 is about 3.2 x 10^21 possible ids, so the link to an unlisted post
 * can't be guessed.
 */
inline constexpr std::size_t k_id_length = 12;

/**
 * @brief Generate a random post id
 *
 *  Example:
 *
 *    make_post_id(); // "7HcPq2mVa0Rn"
 *    make_post_id(); // "Kx04bQmZr9Ta"
 *
 * std::random_device reads from the OS or the CPU's hardware generator
 * (/dev/urandom, getentropy, or RDRAND on Linux, arc4random on macOS). Never
 * a seeded PRNG like rand() or std::mt19937, whose next output can be
 * predicted from previous ones.
 *
 * std::uniform_int_distribution maps each draw onto [0, 61] without bias.
 * The obvious `byte % 62` is biased, since 256 isn't a multiple of 62, so
 * the first 8 characters of the alphabet would come up slightly more often.
 *
 * The device is thread_local since Crow runs routes on multiple threads, and
 * std::random_device isn't guaranteed to be thread-safe.
 *
 * A collision is practically impossible, but the insert still retries with
 * a new id on a primary key failure, see orm::is_duplicate_id.
 */
inline std::string make_post_id()
{
    thread_local std::random_device device{};
    std::uniform_int_distribution<std::size_t> pick{ 0,
        k_id_alphabet.size() - 1 };

    std::string id(k_id_length, '\0');
    std::generate(
        id.begin(), id.end(), [&] { return k_id_alphabet[pick(device)]; });

    return id;
}

/**
 * @brief Check that an id could be a post id
 *
 * Checked before the database lookup. A malformed id gets the same 404 as
 * an unknown one, see openapi.yaml.
 *
 *  Examples:
 *
 *    is_post_id("7HcPq2mVa0Rn");  // true
 *    is_post_id("000000000000");  // true
 *    is_post_id("7HcPq2mVa0R");   // false, 11 characters
 *    is_post_id("7HcPq2mVa0R-");  // false, not base62
 *    is_post_id("../../etc/pa");  // false
 */
constexpr bool is_post_id(std::string_view id)
{
    return id.size() == k_id_length and
           std::all_of(id.begin(), id.end(), [](char c) {
               return k_id_alphabet.find(c) != std::string_view::npos;
           });
}

/****************************************************************************
 * Limits
 *
 * The constraints from openapi.yaml, checked by validate_new_post and the
 * list_posts handler.
 ****************************************************************************/

inline constexpr std::size_t k_body_max_length = 65536;
inline constexpr std::size_t k_title_max_length = 200;
inline constexpr std::size_t k_author_max_length = 64;

inline constexpr std::size_t k_excerpt_length = 100;

inline constexpr std::size_t k_list_limit_default = 20;
inline constexpr std::size_t k_list_limit_max = 100;

// attempts at a fresh id before create_post gives up with a 500
inline constexpr int k_insert_attempts = 3;

// an Idempotency-Key is 16 to 255 visible ASCII characters, kept for 24 hours
inline constexpr std::size_t k_idempotency_key_min_length = 16;
inline constexpr std::size_t k_idempotency_key_max_length = 255;
inline constexpr std::int64_t k_idempotency_ttl_ms = 24LL * 60 * 60 * 1000;

/****************************************************************************
 * Helpers
 ****************************************************************************/

/**
 * @brief A field that failed validation, one entry in a 422's errors
 *
 *  Example:
 *
 *    { "field": "body", "message": "must not be empty" }
 */
struct Field_Error
{
    std::string field{};
    std::string message{};
};

/**
 * @brief Milliseconds since the Unix epoch, for Post::created_at
 *
 * std::chrono::system_clock is wall-clock time, which is what a creation
 * date wants. steady_clock is only for measuring durations.
 */
std::int64_t now_ms();

/**
 * @brief Format a created_at for JSON
 *
 *  Example:
 *
 *    to_iso8601(1761275700123); // "2025-10-24T03:15:00.123Z"
 */
std::string to_iso8601(std::int64_t milliseconds);

/**
 * @brief The excerpt of a post body for listings and link previews
 *
 * Each run of whitespace, newlines included, becomes one space, and the
 * result is cut at k_excerpt_length characters, never inside a UTF-8
 * character. An ellipsis marks a cut, either here or because body_length is
 * longer than body_prefix.
 *
 *  Example:
 *
 *    make_excerpt("# Euler\n\n$$e^{i\\pi} + 1 = 0$$", 29);
 *    // "# Euler $$e^{i\\pi} + 1 = 0$$"
 */
std::string make_excerpt(std::string_view body_prefix,
    std::int64_t body_length);

/**
 * @brief Escape text for an HTML attribute or element
 *
 *  Example:
 *
 *    html_escape("<b>\"x\" & 'y'</b>");
 *    // "&lt;b&gt;&quot;x&quot; &amp; &#39;y&#39;&lt;/b&gt;"
 */
std::string html_escape(std::string_view text);

/**
 * @brief The <title> and Open Graph tags for a post, or catalan's defaults
 *
 * Link previews in chat apps read these from the HTML without running
 * JavaScript. Every value is HTML-escaped, since titles and bodies come from
 * anyone. An unlisted post gets robots noindex.
 *
 *  Example:
 *
 *    <title>Euler's identity · catalan</title>
 *    <meta property="og:title" content="Euler&#39;s identity">
 *    <meta property="og:description" content="by Euler · $$e^{i\pi}...">
 *    <meta property="og:url" content="https://catalan.ly/7HcPq2mVa0Rn">
 */
std::string render_meta(std::optional<orm::Post> const& post,
    std::string_view site_origin);

/**
 * @brief Encode a listing position as the opaque next_cursor string
 *
 *  Example:
 *
 *    { created_at: 1761275700123, id: "7HcPq2mVa0Rn" }
 *
 *  Becomes:
 *
 *    base64url("1761275700123.7HcPq2mVa0Rn"), without = padding
 */
std::string encode_cursor(orm::Cursor const& cursor);

/**
 * @brief Decode a cursor query parameter, nullopt if it's malformed (400)
 *
 * It came from the client, so check everything: the base64url, the integer,
 * and the id with is_post_id.
 */
std::optional<orm::Cursor> decode_cursor(std::string_view cursor);

/**
 * @brief Parse an Idempotency-Key header, nullopt if it's malformed (400)
 *
 * The IETF draft defines the value as a quoted string, the quotes are
 * removed. An unquoted value is accepted too, which is how most clients
 * send it. The key must be hard to guess: keys aren't scoped to a client,
 * since there is no auth, so a client sends a random UUID.
 *
 *  Examples:
 *
 *    parse_idempotency_key("\"8e03978e-40d5-43e8-bc93-6894a57f9324\"");
 *    // "8e03978e-40d5-43e8-bc93-6894a57f9324"
 *
 *    parse_idempotency_key("8e03978e-40d5-43e8-bc93-6894a57f9324");
 *    // "8e03978e-40d5-43e8-bc93-6894a57f9324"
 *
 *    parse_idempotency_key("\"abc\"");      // nullopt, too short
 *    parse_idempotency_key("two words");   // nullopt, not visible ASCII
 */
std::optional<std::string> parse_idempotency_key(std::string_view header);

/**
 * @brief Validate a POST /v1/posts body against NewPost in openapi.yaml
 *
 * Collects every failure, not just the first. An empty result means the body
 * is valid.
 *
 *  * body is required, 1 to k_body_max_length characters
 *
 *  * title and author are optional, up to their k_ limits
 *
 *  * unlisted is an optional boolean
 *
 *  * parent_id is optional, and must pass is_post_id
 *
 *  * no other fields
 */
std::vector<Field_Error> validate_new_post(crow::json::rvalue const& body);

/**
 * @brief Build an RFC 9457 Problem Details response
 *
 * Every error goes through here, so they all have the same shape and the
 * application/problem+json content type. The type URI comes from the
 * status unless one is given.
 *
 *  Example:
 *
 *    problem(404, "Post not found");
 *
 *  Becomes:
 *
 *    404 Not Found
 *    Content-Type: application/problem+json
 *
 *    { "type": "/problems/not-found", "title": "Post not found",
 *      "status": 404 }
 */
crow::response problem(int status,
    std::string_view title,
    std::string_view detail = "",
    std::vector<Field_Error> const& errors = {},
    std::string_view type = "");

/**
 * @brief Post and Post_Summary as the JSON shapes in openapi.yaml
 *
 * created_at goes through to_iso8601, and empty optionals become null.
 */
crow::json::wvalue to_json(orm::Post const& post);
crow::json::wvalue to_json(orm::Post_Summary const& post);

/****************************************************************************
 * Handlers
 *
 * One per endpoint, registered in main.cc. Each follows the numbered steps
 * in the Routes section of decisions.md.
 ****************************************************************************/

/**
 * @brief POST /v1/posts, createPost
 *
 * 201 with a Location header, or 400, 415, 422, 500. With an
 * Idempotency-Key, a retry of the same post replays the original 201 with
 * Idempotent-Replayed: true, and a different post under the same key is a
 * 422.
 */
crow::response create_post(orm::Database& db, crow::request const& req);

/**
 * @brief GET /v1/posts, listPosts
 *
 * 200 with a page of posts, or 400 for a bad limit, order, or cursor.
 */
crow::response list_posts(orm::Database& db, crow::request const& req);

/**
 * @brief GET /v1/posts/{id}, getPost
 *
 * 200 with ETag and Cache-Control, 304 for a matching If-None-Match, or 404.
 */
crow::response get_post(orm::Database& db,
    crow::request const& req,
    std::string const& id);

/**
 * @brief GET /v1/posts/{id}/raw, getPostRaw
 *
 * Same as get_post, with only the body as text/plain.
 */
crow::response get_post_raw(orm::Database& db,
    crow::request const& req,
    std::string const& id);

/**
 * @brief GET /meta/{id}, getPostMeta
 *
 * The meta tags for the site's HTML, filled in by the proxy's templates for
 * link previews. Always 200: an unknown id or a database failure gets the
 * default tags, since an error status would fail the whole page.
 */
crow::response get_post_meta(orm::Database& db,
    std::string const& id,
    std::string_view site_origin);

/**
 * @brief GET /health, getHealth
 *
 * 200 "ok", or 503 Problem Details if the database is unreachable.
 */
crow::response get_health(orm::Database& db);

} // namespace catalan::api
