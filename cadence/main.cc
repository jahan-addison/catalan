#include <cadence/hello.h> // for Greeter
#include <fmt/base.h>      // for println

int main()
{
    auto greeter = cadence::Greeter{};
    fmt::println("Greet: {}", greeter.greet("John"));
}