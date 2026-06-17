#pragma once

#include <imr/mold/types.h>
#include <imr/mold/packet_builder.h>
#include <imr/mold/downstream/pacer.h>

#include <span>
#include <chrono>
#include <algorithm>
#include <array>

namespace test_common
{
    using namespace imr;

    constexpr auto min_message_size{mold::PacketBuilder::min_message_size};
    // Body size = total - 2-byte length prefix
    constexpr mold::types::LengthPrefix itch_min_body_size{min_message_size -
                                                           static_cast<std::uint16_t>(sizeof(mold::types::LengthPrefix))};

    consteval void write_len_prefix(std::span<char> message)
    {
        const auto bytes{std::bit_cast<std::array<char, sizeof(mold::types::LengthPrefix)>>(
            std::byteswap(itch_min_body_size))}; // was: min_message_size
        std::ranges::copy(bytes, message.begin());
    }

    consteval void copy_timestamp(std::chrono::nanoseconds timestamp, std::span<char> message)
    {
        // convert to 6-byte big endian
        auto count{std::byteswap(timestamp.count()) >> 16};

        const auto bytes{std::bit_cast<std::array<char, sizeof(std::uint64_t)>>(count)};

        static constexpr auto timestamp_offset{7UZ};
        static constexpr auto timestamp_size{6UZ};

        std::ranges::copy_n(bytes.data(), timestamp_size, message.begin() + timestamp_offset);
    }

    template <std::size_t N>
    consteval std::array<char, N * min_message_size> create_test_file(std::chrono::nanoseconds timestamp_increment)
    {
        std::array<char, N * min_message_size> file{};
        auto timestamp{std::chrono::nanoseconds{mold::downstream::market_pre}};

        for (auto i{0UZ}; i < N; ++i)
        {
            const std::span message{std::span{file}.subspan(i * min_message_size, min_message_size)};

            write_len_prefix(message);
            copy_timestamp(timestamp, message);

            timestamp += timestamp_increment;
        }
        return file;
    }
}
