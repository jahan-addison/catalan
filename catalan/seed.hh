/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <catalan/examples.hh> // for k_euler_identity, k_basel_problem
#include <catalan/manage.hh>   // for Storage, Post, insert_post
#include <cstdint>             // for int64_t
#include <string>              // for basic_string, string
#include <string_view>         // for string_view

/****************************************************************************
 * Seed
 *
 * The two example posts a new database starts with, from the files in
 * examples/. They're the oldest posts, so they sit last in listings, at
 * fixed ids so their links are the same on every deployment.
 *
 *  Example:
 *
 *    /EulerFormula   Euler's identity
 *    /BaselProblem   The Basel problem, also the editor's example
 *
 * Only a new database is seeded, see Database::is_new. An operator who
 * removes a seed post doesn't get it back on the next restart.
 ****************************************************************************/

namespace catalan::orm {

inline constexpr std::string_view k_euler_identity_id = "EulerFormula";
inline constexpr std::string_view k_basel_problem_id = "BaselProblem";

/**
 * @brief Insert the example posts, Euler first, then Basel a millisecond
 *  later, in one transaction
 */
inline void seed_examples(Storage& storage, std::int64_t created_at)
{
    auto const example = [](std::string_view id,
                             std::string_view title,
                             std::string_view body,
                             std::int64_t at) {
        Post post{};
        post.id = std::string{ id };
        post.created_at = at;
        post.title = std::string{ title };
        post.author = "catalan";
        post.body = std::string{ body };
        return post;
    };

    auto guard = storage.transaction_guard();
    insert_post(storage,
        example(k_euler_identity_id,
            "Euler's identity",
            examples::k_euler_identity,
            created_at));
    insert_post(storage,
        example(k_basel_problem_id,
            "The Basel problem",
            examples::k_basel_problem,
            created_at + 1));
    guard.commit();
}

} // namespace catalan::orm
