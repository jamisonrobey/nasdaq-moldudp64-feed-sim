#pragma once

#include <cstdint>
#include <array>

namespace imr::mold::types
{
    namespace header
    {
        using Session = std::array<char, 10>;
        inline constexpr std::size_t session_offset{0};

        using SequenceNumber = std::uint64_t;
        inline constexpr std::size_t sequence_number_offset{sizeof(Session)};

        using MessageCount = std::uint16_t;
        inline constexpr std::size_t message_count_offset{sizeof(Session) + sizeof(SequenceNumber)};

        inline constexpr auto length{sizeof(Session) + sizeof(SequenceNumber) + sizeof(MessageCount)};
    }

    using LengthPrefix = std::uint16_t;
}
