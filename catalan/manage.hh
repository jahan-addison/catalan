/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <cstdint>                 // for int64_t
#include <mutex>                   // for mutex, scoped_lock
#include <optional>                // for optional
#include <sqlite3.h>               // for SQLITE_CONSTRAINT_PRIMARYKEY, sqlite3
#include <sqlite_orm/sqlite_orm.h> // for make_storage, make_table, make_column
#include <stdexcept>               // for runtime_error
#include <string>                  // for basic_string, string, to_string
#include <system_error>            // for system_error
#include <utility>                 // for forward

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

using Storage = decltype(initialize_storage(""));

/**
 * @brief Schema version, stored in the database file as `PRAGMA user_version`
 *
 * Bump it whenever the schema above changes, and add a migration step to
 * migrate().
 */
inline constexpr int k_schema_version = 1;

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

    // Migration steps go here, oldest first. For example, when
    // k_schema_version becomes 2:
    //
    //   if (version < 2) {
    //       storage.transaction([&] {
    //           // ALTER TABLE, backfill, CREATE INDEX, ...
    //           storage.pragma.user_version(2);
    //           return true;
    //       });
    //   }

    if (version < k_schema_version) {
        throw std::runtime_error(
            "no migration from schema version " + std::to_string(version));
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
