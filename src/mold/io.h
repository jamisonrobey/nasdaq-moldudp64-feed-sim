#pragma once

#include <span>
#include <cstddef>

namespace imr::mold::io
{
    [[nodiscard]]
    std::span<const char> read_message(std::span<const char> bytes, std::size_t& pos) noexcept;
}
