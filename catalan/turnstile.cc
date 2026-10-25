/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <catalan/turnstile.hh>

#include <crow.h>      // for json, CROW_LOG_ERROR
#include <cstddef>     // for size_t
#include <curl/curl.h> // for curl_easy_init, curl_easy_setopt, CURL
#include <memory>      // for unique_ptr
#include <string>      // for basic_string, string
#include <string_view> // for string_view
#include <utility>     // for move

/****************************************************************************
 * Turnstile
 *
 * libcurl calls to siteverify, see turnstile.hh.
 ****************************************************************************/

namespace catalan::turnstile {

namespace {

std::size_t append_body(char* data,
    std::size_t size,
    std::size_t count,
    void* body)
{
    static_cast<std::string*>(body)->append(data, size * count);
    return size * count;
}

} // namespace

Result parse_response(std::string_view body)
{
    auto const json = crow::json::load(std::string{ body });
    if (not json or json.t() != crow::json::type::Object or
        not json.has("success"))
        return Result::unavailable;

    auto const success = json["success"].t();
    if (success == crow::json::type::True)
        return Result::passed;
    if (success == crow::json::type::False)
        return Result::failed;
    return Result::unavailable;
}

std::string form_encode(std::string_view value)
{
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string encoded{};
    for (char const c : value) {
        auto const byte = static_cast<unsigned char>(c);
        bool const unreserved = (byte >= 'A' and byte <= 'Z') or
                                (byte >= 'a' and byte <= 'z') or
                                (byte >= '0' and byte <= '9') or c == '-' or
                                c == '_' or c == '.' or c == '~';
        if (unreserved) {
            encoded += c;
        } else {
            encoded += '%';
            encoded += hex[byte >> 4];
            encoded += hex[byte & 0x0F];
        }
    }
    return encoded;
}

std::string form_body(std::string_view secret,
    std::string_view token,
    std::string_view remote_ip)
{
    auto body =
        "secret=" + form_encode(secret) + "&response=" + form_encode(token);
    if (not remote_ip.empty())
        body += "&remoteip=" + form_encode(remote_ip);
    return body;
}

void initialize()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

Verifier make_verifier(std::string secret, std::string endpoint)
{
    return [secret = std::move(secret), endpoint = std::move(endpoint)](
               std::string_view token, std::string_view remote_ip) {
        if (token.empty())
            return Result::failed;

        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl{
            curl_easy_init(), &curl_easy_cleanup
        };
        if (not curl)
            return Result::unavailable;

        auto const request = form_body(secret, token, remote_ip);
        std::string response{};
        curl_easy_setopt(curl.get(), CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, request.c_str());
        curl_easy_setopt(curl.get(),
            CURLOPT_POSTFIELDSIZE,
            static_cast<long>(request.size()));
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, append_body);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 5L);
        // signals would interrupt other threads, the timeout doesn't need them
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);

        if (auto const code = curl_easy_perform(curl.get()); code != CURLE_OK) {
            CROW_LOG_ERROR << "turnstile: " << curl_easy_strerror(code);
            return Result::unavailable;
        }

        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status != 200) {
            CROW_LOG_ERROR << "turnstile: siteverify answered " << status;
            return Result::unavailable;
        }
        return parse_response(response);
    };
}

} // namespace catalan::turnstile
