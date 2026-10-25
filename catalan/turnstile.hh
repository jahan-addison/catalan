/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#pragma once

#include <functional>  // for function
#include <string>      // for basic_string, string
#include <string_view> // for string_view

/****************************************************************************
 * Turnstile
 *
 * Server-side verification of Cloudflare Turnstile tokens, the human check
 * on POST /v1/posts. The browser's widget produces a one-time token, which
 * is only trusted once Cloudflare's siteverify endpoint accepts it with the
 * secret key.
 *
 *  Example:
 *
 *    POST https://challenges.cloudflare.com/turnstile/v0/siteverify
 *    secret=<secret>&response=<token>&remoteip=203.0.113.7
 *
 *  Becomes:
 *
 *    { "success": true, ... }    // Result::passed
 *
 ****************************************************************************/

namespace catalan::turnstile {

inline constexpr std::string_view k_siteverify_url =
    "https://challenges.cloudflare.com/turnstile/v0/siteverify";

enum class Result
{
    passed,
    // the token is missing, wrong, expired, or already used, 403
    failed,
    // Cloudflare couldn't be reached or answered nonsense, 503
    unavailable
};

/**
 * @brief Check a token for a client IP
 *
 * A function, so tests swap in one that passes or fails without the network.
 * An empty Verifier means verification is off, see main.cc.
 */
using Verifier =
    std::function<Result(std::string_view token, std::string_view remote_ip)>;

/**
 * @brief Read a siteverify response body
 *
 *  Examples:
 *
 *    parse_response(R"({"success": true})");    // passed
 *    parse_response(R"({"success": false})");   // failed
 *    parse_response("<html>502</html>");         // unavailable
 */
Result parse_response(std::string_view body);

/**
 * @brief Percent-encode a value for an application/x-www-form-urlencoded body
 *
 *  Example:
 *
 *    form_encode("a b+c/d");  // "a%20b%2Bc%2Fd"
 */
std::string form_encode(std::string_view value);

/**
 * @brief The request body for siteverify
 *
 * remoteip is left out when empty, Cloudflare treats it as optional.
 */
std::string form_body(std::string_view secret,
    std::string_view token,
    std::string_view remote_ip);

/**
 * @brief Initialize libcurl once, before any request threads start
 */
void initialize();

/**
 * @brief A Verifier that calls siteverify with libcurl
 *
 * Each check is its own request with a 5 second timeout. A network error,
 * a non-200 status, or an unreadable body is Result::unavailable.
 */
Verifier make_verifier(std::string secret,
    std::string endpoint = std::string{ k_siteverify_url });

} // namespace catalan::turnstile
