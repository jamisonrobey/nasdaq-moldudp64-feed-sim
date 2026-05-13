#pragma once

#include <cstdint>
#include <array>

namespace imr::mold
{
    using Session = std::array<char, 10>;
    using SequenceNumber = std::uint64_t;
    using MessageCount = std::uint16_t;

    inline constexpr std::size_t header_length{sizeof(Session) + sizeof(SequenceNumber) + sizeof(MessageCount)};

    using LengthPrefix = std::uint16_t;
};
