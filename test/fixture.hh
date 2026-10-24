/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <catalan/api.hh> // for make_post_id
#include <filesystem>     // for path, temp_directory_path, remove
#include <string>         // for basic_string, string

/****************************************************************************
 * Test fixtures
 *
 * The Database tests use a real file rather than ":memory:", since
 * sqlite_orm opens ":memory:" before on_open runs, so it wouldn't get
 * extended result codes and wouldn't behave like production.
 ****************************************************************************/

namespace catalan::test {

/**
 * @brief Temporary database file path
 *
 * Named with a random post id, so test cases running in parallel under
 * ctest -j never share a file. Removed with its WAL files on construction
 * and destruction.
 *
 *  Example:
 *
 *    /tmp/catalan-test-7HcPq2mVa0Rn.db
 */
struct Temp_Database_Path
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("catalan-test-" + api::make_post_id() + ".db");

    Temp_Database_Path() { remove(); }
    ~Temp_Database_Path() { remove(); }

    Temp_Database_Path(Temp_Database_Path const&) = delete;
    Temp_Database_Path& operator=(Temp_Database_Path const&) = delete;

    std::string string() const { return path.string(); }

    void remove() const
    {
        for (auto const* suffix : { "", "-wal", "-shm" })
            std::filesystem::remove(path.string() + suffix);
    }
};

} // namespace catalan::test
