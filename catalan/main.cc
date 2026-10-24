/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

#include <catalan/api.hh>    // for create_post, list_posts, get_post, ...
#include <catalan/manage.hh> // for Database
#include <crow.h>            // for SimpleApp, CROW_ROUTE, CROW_LOG_CRITICAL
#include <cstdlib>           // for getenv
#include <exception>         // for exception
#include <string>            // for basic_string, string, stoi

namespace {

std::string env_or(char const* name, std::string const& fallback)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at startup
    if (char const* value = std::getenv(name)) {
        return value;
    }
    return fallback;
}

} // namespace

int main()
{
    crow::SimpleApp app;

    // Startup failures (unopenable database, bad CATALAN_PORT) are logged
    // and exit 1, so `docker logs` shows why the container died. Crow
    // doesn't throw on a port already in use, it logs that itself
    try {
        catalan::orm::Database db{ env_or("CATALAN_DB", "catalan.db") };

        // routes only forward to api.hh, which the tests can reach

        CROW_ROUTE(app, "/v1/posts")
            .methods("POST"_method)([&db](crow::request const& req) {
                return catalan::api::create_post(db, req);
            });

        CROW_ROUTE(app, "/v1/posts")
            .methods("GET"_method)([&db](crow::request const& req) {
                return catalan::api::list_posts(db, req);
            });

        CROW_ROUTE(app, "/v1/posts/<string>")
            .methods("GET"_method)(
                [&db](crow::request const& req, std::string const& id) {
                    return catalan::api::get_post(db, req, id);
                });

        CROW_ROUTE(app, "/v1/posts/<string>/raw")
            .methods("GET"_method)(
                [&db](crow::request const& req, std::string const& id) {
                    return catalan::api::get_post_raw(db, req, id);
                });

        CROW_ROUTE(app, "/health")(
            [&db] { return catalan::api::get_health(db); });

        app.bindaddr("0.0.0.0")
            .port(std::stoi(env_or("CATALAN_PORT", "18080")))
            .multithreaded()
            .run();
    } catch (std::exception const& e) {
        CROW_LOG_CRITICAL << "catalan failed to start: " << e.what();
        return 1;
    }
}
