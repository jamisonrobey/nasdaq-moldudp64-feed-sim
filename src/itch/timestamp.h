#pragma once

#include <span>
#include <chrono>

namespace itch
{
    [[nodiscard]]
    std::chrono::nanoseconds extract_timestamp(std::span<const char> bytes);
};
