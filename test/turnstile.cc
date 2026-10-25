/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <doctest/doctest.h> // for ResultBuilder, CHECK, TEST_CASE

#include <catalan/turnstile.hh> // for parse_response, form_encode, Result

using catalan::turnstile::Result;

TEST_CASE("parse_response")
{
    using catalan::turnstile::parse_response;
    CHECK_EQ(parse_response(R"({"success": true, "hostname": "example.com"})"),
        Result::passed);
    CHECK_EQ(
        parse_response(
            R"({"success": false, "error-codes": ["invalid-input-response"]})"),
        Result::failed);
    CHECK_EQ(
        parse_response("<html>502 Bad Gateway</html>"), Result::unavailable);
    CHECK_EQ(
        parse_response(R"({"hostname": "example.com"})"), Result::unavailable);
    CHECK_EQ(parse_response(R"({"success": "yes"})"), Result::unavailable);
    CHECK_EQ(parse_response(""), Result::unavailable);
}

TEST_CASE("form_encode")
{
    using catalan::turnstile::form_encode;
    CHECK_EQ(form_encode("abcXYZ019-_.~"), "abcXYZ019-_.~");
    CHECK_EQ(form_encode("a b+c/d&e=f"), "a%20b%2Bc%2Fd%26e%3Df");
    CHECK_EQ(form_encode("∑"), "%E2%88%91");
}

TEST_CASE("form_body")
{
    using catalan::turnstile::form_body;
    CHECK_EQ(form_body("s3cret", "to&ken", "203.0.113.7"),
        "secret=s3cret&response=to%26ken&remoteip=203.0.113.7");
    CHECK_EQ(form_body("s", "t", ""), "secret=s&response=t");
}
