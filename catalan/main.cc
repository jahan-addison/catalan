#include <crow.h>  // for SimpleApp, CROW_ROUTE
#include <cstdlib> // for getenv
#include <string>  // for stoi

namespace {

int env_port(int fallback)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at startup
    if (const char* port = std::getenv("CATALAN_PORT")) {
        return std::stoi(port);
    }
    return fallback;
}

} // namespace

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/health")([] { return "ok"; });

    app.bindaddr("0.0.0.0").port(env_port(18080)).multithreaded().run();
}
