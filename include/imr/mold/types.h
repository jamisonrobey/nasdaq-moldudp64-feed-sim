#pragma once

#include <cstdint>
#include <array>

namespace imr::mold::types
{
    namespace header
    {
        using Session = std::array<char, 10>;
        using SequenceNumber = std::uint64_t;
        using MessageCount = std::uint16_t;

        inline constexpr std::size_t session_offset{0};
        inline constexpr std::size_t sequence_number_offset{sizeof(Session)};
        inline constexpr std::size_t message_count_offset{sequence_number_offset + sizeof(SequenceNumber)};
        inline constexpr std::size_t message_block_offset{message_count_offset + sizeof(MessageCount)};

        inline constexpr std::size_t length{sizeof(Session) + sizeof(SequenceNumber) + sizeof(MessageCount)};
    }

    using LengthPrefix = std::uint16_t;
}
