/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <cstddef>                 // for size_t
#include <cstdint>                 // for int64_t
#include <mutex>                   // for mutex, scoped_lock
#include <optional>                // for optional
#include <sqlite3.h>               // for SQLITE_CONSTRAINT_PRIMARYKEY, sqlite3
#include <sqlite_orm/sqlite_orm.h> // for make_storage, make_table, make_column
#include <stdexcept>               // for runtime_error
#include <string>                  // for basic_string, string, to_string
#include <system_error>            // for system_error
#include <tuple>                   // for apply
#include <utility>                 // for forward
#include <vector>                  // for vector

/****************************************************************************
 * Manage
 *
 * SQLite storage via sqlite_orm. The schema, migrations, the Database
 * connection, and the queries the routes in main.cc call. Nothing here
 * knows about HTTP or logs, failures throw std::system_error (sqlite_orm)
 * or std::runtime_error (migrate) for main.cc to handle.
 *
 *  Example schema:
 *
 *  struct Post
 *  {
 *      std::string id;
 *      std::int64_t created_at;
 *      bool unlisted;
 *      std::optional<std::string> parent_id;
 *      ...
 *  };
 *
 *  Becomes:
 *
 *  CREATE TABLE "posts" ("id" TEXT PRIMARY KEY NOT NULL,
 *    "created_at" INTEGER NOT NULL, "unlisted" INTEGER DEFAULT (0) NOT NULL,
 *    "parent_id" TEXT NULL, ...,
 *    FOREIGN KEY("parent_id") REFERENCES "posts"("id"));
 *  CREATE INDEX "posts_listing" ON "posts" ("unlisted", "created_at", "id");
 *
 ****************************************************************************/

namespace catalan::orm {

struct Post
{
    std::string id{};
    // milliseconds since the Unix epoch in UTC, formatted as ISO 8601 by
    // api.hh. An integer sorts correctly and needs no parsing
    std::int64_t created_at = 0;
    bool unlisted = false;
    std::optional<std::string> parent_id{};
    std::optional<std::string> title{};
    std::optional<std::string> author{};
    std::string body{};
};

/**
 * @brief A listing row, a Post without body or unlisted
 *
 * unlisted is always false in a listing, and leaving out body keeps a page
 * small no matter how long its posts are.
 */
struct Post_Summary
{
    std::string id{};
    std::int64_t created_at = 0;
    std::optional<std::string> parent_id{};
    std::optional<std::string> title{};
    std::optional<std::string> author{};
};

/**
 * @brief A position in the listing, the last row of the previous page
 *
 * id breaks ties between posts created in the same millisecond, so every
 * position is unique. api.hh encodes it as the opaque next_cursor string.
 */
struct Cursor
{
    std::int64_t created_at = 0;
    std::string id{};
};

enum class Order
{
    desc,
    asc
};

/**
 * @brief One page of the listing, next is nullopt on the last page
 */
struct Page
{
    std::vector<Post_Summary> posts{};
    std::optional<Cursor> next{};
};

/**
 * @brief An Idempotency-Key and the post it created
 *
 * created_at is when the key was first used, keys older than the TTL in
 * api.hh are removed on the next keyed insert.
 */
struct Idempotency_Key
{
    std::string key{};
    std::string post_id{};
    std::int64_t created_at = 0;
};

/**
 * @brief Construct the storage and schema for the posts table
 *
 * Building the storage doesn't open or create the database file, that
 * happens on first use.
 *
 *  * parent_id is a foreign key, so an edit of a post that doesn't exist
 *    fails with a constraint error (422 in the API). SQLite leaves foreign
 *    keys off by default, sqlite_orm turns on `PRAGMA foreign_keys` for every
 *    connection when a table declares one
 *
 *  * posts_listing is the index for GET /v1/posts, in the same column order
 *    as the query:
 *
 *    WHERE unlisted = 0 AND (created_at, id) < (?, ?)
 *    ORDER BY created_at DESC, id DESC
 */
inline auto initialize_storage(std::string const& db_path)
{
    using namespace sqlite_orm;
    return make_storage(db_path,
        make_index(
            "posts_listing", &Post::unlisted, &Post::created_at, &Post::id),
        make_index("idempotency_keys_created_at", &Idempotency_Key::created_at),
        make_table("posts",
            make_column("id", &Post::id, primary_key()),
            make_column("created_at", &Post::created_at),
            make_column("unlisted", &Post::unlisted, default_value(false)),
            make_column("parent_id", &Post::parent_id),
            make_column("title", &Post::title),
            make_column("author", &Post::author),
            make_column("body", &Post::body),
            foreign_key(&Post::parent_id).references(&Post::id)),
        make_table("idempotency_keys",
            make_column("key", &Idempotency_Key::key, primary_key()),
            make_column("post_id", &Idempotency_Key::post_id),
            make_column("created_at", &Idempotency_Key::created_at),
            foreign_key(&Idempotency_Key::post_id).references(&Post::id)));
}

using Storage = decltype(initialize_storage(""));

/**
 * @brief Schema version, stored in the database file as `PRAGMA user_version`
 *
 * Bump it whenever the schema above changes, and add a migration step to
 * migrate().
 */
inline constexpr int k_schema_version = 2;

/**
 * @brief Apply a migration step that only adds tables or indexes
 *
 * sync_schema() is safe on an existing database only when it would create
 * and nothing else, so the simulation is checked first. The schema change
 * and the new user_version commit together, or not at all.
 */
inline void migrate_additive(Storage& storage, int to_version)
{
    using namespace sqlite_orm;
    for (auto const& [name, result] : storage.sync_schema_simulate()) {
        if (result != sync_schema_result::already_in_sync and
            result != sync_schema_result::new_table_created) {
            throw std::runtime_error(
                "migration to schema version " + std::to_string(to_version) +
                " would change '" + name + "', it must only add");
        }
    }

    auto guard = storage.transaction_guard();
    storage.sync_schema();
    storage.pragma.user_version(to_version);
    guard.commit();
}

/**
 * @brief Migrate the database to k_schema_version, or throw
 *
 * sync_schema() only runs on a new database. On an existing one it may drop
 * and recreate a table whose columns no longer match the struct, deleting
 * every row, so schema changes go through versioned migration steps instead.
 *
 *  user_version:
 *
 *    0                    new database, create everything
 *    < k_schema_version   run each migration step in order
 *    == k_schema_version  verify the schema matches, change nothing
 *    > k_schema_version   database is from a newer build, throw
 *
 *  Steps:
 *
 *    1 -> 2  add idempotency_keys
 */
inline void migrate(Storage& storage)
{
    using namespace sqlite_orm;
    auto const version = storage.pragma.user_version();

    if (version > k_schema_version) {
        throw std::runtime_error(
            "database schema version " + std::to_string(version) +
            " is newer than this build's " + std::to_string(k_schema_version));
    }

    if (version == 0) {
        // a table without a version came from somewhere else, we don't let
        // sync_schema() decide what to do with its rows
        if (storage.table_exists("posts")) {
            throw std::runtime_error(
                "database has a posts table but no schema version, "
                "refusing to touch it");
        }
        storage.sync_schema();
        storage.pragma.user_version(k_schema_version);
        return;
    }

    // migration steps, oldest first, each one sets user_version
    if (version < 2)
        migrate_additive(storage, 2);

    if (auto const migrated = storage.pragma.user_version();
        migrated != k_schema_version) {
        throw std::runtime_error(
            "no migration from schema version " + std::to_string(migrated));
    }

    // catches a struct or column change made without bumping the version
    for (auto const& [name, result] : storage.sync_schema_simulate()) {
        if (result != sync_schema_result::already_in_sync) {
            throw std::runtime_error("schema of '" + name +
                                     "' doesn't match the database, bump "
                                     "k_schema_version and add a migration");
        }
    }
}

/**
 * @brief Database connection behind a mutex, open for the life of the server
 *
 *  * open_forever() keeps one connection open, instead of sqlite_orm's
 *    default of opening and closing one per call. The per-connection pragmas
 *    below only stick because of it
 *
 *  * WAL lets readers keep reading while a write is in progress, and is
 *    saved in the database file
 *
 *  * busy_timeout makes a blocked write wait up to 5s instead of failing
 *    with SQLITE_BUSY
 *
 *  * Extended result codes make a constraint failure say which constraint,
 *    see is_duplicate_id and is_unknown_parent. Not for ":memory:", which
 *    sqlite_orm opens in its constructor before on_open is set
 *
 *  * Crow handles requests on multiple threads and sqlite_orm doesn't
 *    promise a storage is safe to share, so every access goes through
 *    with_storage and the mutex. One storage per thread is the upgrade path
 *    if reads become the bottleneck
 *
 *  Example:
 *
 *    catalan::orm::Database db{ "catalan.db" };
 *
 *    auto post = db.with_storage([&](auto& storage) {
 *        return storage.template get_optional<catalan::orm::Post>(id);
 *    });
 */
class Database
{
  public:
    explicit Database(std::string const& db_path)
        : storage_(initialize_storage(db_path))
    {
        storage_.on_open = [](sqlite3* db) {
            sqlite3_extended_result_codes(db, 1);
        };
        storage_.open_forever();
        storage_.busy_timeout(5000);
        storage_.pragma.journal_mode(sqlite_orm::journal_mode::WAL);
        migrate(storage_);
    }

    Database(Database const&) = delete;
    Database& operator=(Database const&) = delete;

    /**
     * @brief Run fn(storage) while holding the lock and return its result
     *
     * Keep fn to database calls only, every other request waits on it.
     */
    template<typename F>
    decltype(auto) with_storage(F&& fn)
    {
        std::scoped_lock lock{ mutex_ };
        return std::forward<F>(fn)(storage_);
    }

  private:
    Storage storage_;
    std::mutex mutex_{};
};

/****************************************************************************
 * Queries
 *
 * One query each, called inside Database::with_storage. Inputs are already
 * validated by api.hh, the database constraints are a backstop. Failures
 * throw std::system_error for the handler to map to a response.
 *
 * See the Storage API section of decisions.md.
 ****************************************************************************/

/**
 * @brief Insert a new post
 *
 * One INSERT with an explicit column list. A plain insert(post) won't
 * compile for a text primary key, and replace() would silently overwrite a
 * duplicate id.
 *
 *  Throws std::system_error:
 *
 *    is_duplicate_id(e)    // the id is taken, the handler retries
 *    is_unknown_parent(e)  // parent_id doesn't exist, 422
 */
inline void insert_post(Storage& storage, Post const& post)
{
    using namespace sqlite_orm;
    storage.insert(post,
        columns(&Post::id,
            &Post::created_at,
            &Post::unlisted,
            &Post::parent_id,
            &Post::title,
            &Post::author,
            &Post::body));
}

/**
 * @brief Get a post by id, including unlisted posts
 *
 * Unlisted only hides a post from the listing, not from its link.
 */
inline std::optional<Post> get_post(Storage& storage, std::string const& id)
{
    return storage.get_optional<Post>(id);
}

/**
 * @brief Get one page of listed posts, after a cursor
 *
 * Fetch limit + 1 rows, if the extra row comes back there's another page.
 * Drop it, and next is the last kept row's (created_at, id).
 *
 *  Example - order desc, after (t, i):
 *
 *    WHERE unlisted = 0
 *      AND (created_at < t OR (created_at = t AND id < i))
 *    ORDER BY created_at DESC, id DESC
 *    LIMIT limit + 1
 *
 * For order asc, flip both < to > and both DESC to ASC. With no cursor, drop
 * the second condition.
 */
inline Page list_posts(Storage& storage,
    std::size_t limit,
    Order order,
    std::optional<Cursor> const& after)
{
    using namespace sqlite_orm;

    auto const fetch = static_cast<int>(limit) + 1;
    auto const listed = c(&Post::unlisted) == false;

    auto const select_page = [&](auto condition, auto first, auto second) {
        return storage.select(columns(&Post::id,
                                  &Post::created_at,
                                  &Post::parent_id,
                                  &Post::title,
                                  &Post::author),
            where(condition),
            multi_order_by(first, second),
            sqlite_orm::limit(fetch));
    };

    auto rows = [&] {
        if (order == Order::desc) {
            auto const first = order_by(&Post::created_at).desc();
            auto const second = order_by(&Post::id).desc();
            if (not after)
                return select_page(listed, first, second);
            return select_page(
                listed and (c(&Post::created_at) < after->created_at or
                               (c(&Post::created_at) == after->created_at and
                                   c(&Post::id) < after->id)),
                first,
                second);
        }
        auto const first = order_by(&Post::created_at).asc();
        auto const second = order_by(&Post::id).asc();
        if (not after)
            return select_page(listed, first, second);
        return select_page(
            listed and (c(&Post::created_at) > after->created_at or
                           (c(&Post::created_at) == after->created_at and
                               c(&Post::id) > after->id)),
            first,
            second);
    }();

    Page page{};
    page.posts.reserve(rows.size());
    for (auto& row : rows) {
        std::apply(
            [&](auto& id,
                auto& created_at,
                auto& parent_id,
                auto& title,
                auto& author) {
                page.posts.push_back(Post_Summary{ std::move(id),
                    created_at,
                    std::move(parent_id),
                    std::move(title),
                    std::move(author) });
            },
            row);
    }

    // the extra row only signals a next page, it belongs to the next one
    if (page.posts.size() > limit) {
        page.posts.resize(limit);
        auto const& last = page.posts.back();
        page.next = Cursor{ last.created_at, last.id };
    }

    return page;
}

/**
 * @brief Insert a post under an Idempotency-Key, or get the key's post
 *
 * Returns nullopt when the post was inserted, or the post the key already
 * created. The handler decides whether that's a replay or a mismatch.
 *
 * Expired keys are removed, the key is looked up, and the post and key are
 * inserted in one transaction, under Database's mutex. Two requests with the
 * same key can't interleave, so there is no in-progress state to report.
 *
 *  Example:
 *
 *    insert_post_once(storage, post, key, cutoff);  // nullopt, inserted
 *    insert_post_once(storage, post, key, cutoff);  // Post, already used
 *
 *  Throws std::system_error, as insert_post does.
 */
inline std::optional<Post> insert_post_once(Storage& storage,
    Post const& post,
    std::string const& key,
    std::int64_t expired_before)
{
    using namespace sqlite_orm;
    auto guard = storage.transaction_guard();

    storage.remove_all<Idempotency_Key>(
        where(c(&Idempotency_Key::created_at) < expired_before));

    if (auto const used = storage.get_optional<Idempotency_Key>(key)) {
        auto existing = storage.get<Post>(used->post_id);
        guard.commit();
        return existing;
    }

    insert_post(storage, post);
    storage.insert(Idempotency_Key{ key, post.id, post.created_at },
        columns(&Idempotency_Key::key,
            &Idempotency_Key::post_id,
            &Idempotency_Key::created_at));
    guard.commit();
    return std::nullopt;
}

/**
 * @brief Check the database is reachable, or throw
 *
 * Reads PRAGMA user_version, which is cheap and touches the database file.
 */
inline void check_health(Storage& storage)
{
    static_cast<void>(storage.pragma.user_version());
}

/**
 * @brief Check which constraint an insert broke
 *
 * Maps sqlite_orm's std::system_error to a response, and needs the extended
 * result codes that Database turns on.
 *
 *  Example:
 *
 *    try {
 *        db.with_storage([&](auto& storage) { insert_post(storage, post); });
 *    } catch (std::system_error const& e) {
 *        if (is_duplicate_id(e))   // the generated id is taken, retry
 *        if (is_unknown_parent(e)) // parent_id doesn't exist, 422
 *    }
 */
inline bool is_constraint(std::system_error const& error, int code)
{
    return error.code().category() ==
               sqlite_orm::get_sqlite_error_category() and
           error.code().value() == code;
}

inline bool is_duplicate_id(std::system_error const& error)
{
    return is_constraint(error, SQLITE_CONSTRAINT_PRIMARYKEY);
}

inline bool is_unknown_parent(std::system_error const& error)
{
    return is_constraint(error, SQLITE_CONSTRAINT_FOREIGNKEY);
}

} // namespace catalan::orm
