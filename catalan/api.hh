/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <algorithm>   // for all_of, generate
#include <cstddef>     // for size_t
#include <random>      // for random_device, uniform_int_distribution
#include <string>      // for basic_string, string
#include <string_view> // for basic_string_view, string_view

/****************************************************************************
 * API
 *
 * Request and response logic for the HTTP routes registered in main.cc.
 * Post ids, validation, and the JSON shapes in openapi.yaml.
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

} // namespace catalan::api
