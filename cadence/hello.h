#pragma once

#include <string>      // for basic_string
#include <string_view> // for basic_string_view

namespace cadence {

struct Greeter
{
    std::string greet(std::string_view person);
};

} // namespace cadence