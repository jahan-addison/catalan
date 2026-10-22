#include <cadence/hello.h>

#include <fmt/format.h> // for format
#include <matchit.h>    // for pattern, _, PatternPipable
#include <string>       // for basic_string, string
#include <string_view>  // for basic_string_view, string_view

namespace cadence {

std::string Greeter::greet(std::string_view person)
{
    namespace m = matchit;

    return m::match(person)(
        m::pattern | "Bob" = []() -> std::string { return "Hello, Bob!"; },
        m::pattern | "Sarah" = []() -> std::string { return "Howdy, Sarah!"; },
        // default case
        m::pattern | m::_ = [&, person]() -> std::string {
            return fmt::format("Hi, {}!", person);
        });
}

} // namespace cadence