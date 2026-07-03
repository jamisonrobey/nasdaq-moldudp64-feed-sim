#pragma once

#include "test_file_fixture.h"

#include <imr/mold/types.h>
#include <imr/mold/packet_builder.h>
#include <imr/mold/downstream/pacer.h>

#include <span>
#include <chrono>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace test_common
{
    template <std::size_t NumMessages, std::int64_t TimestampIncrementNs = 1>
    class ItchFileFixture : public TestFileFixture<ItchFileFixture<NumMessages, TimestampIncrementNs>>
    {
      public:
        static consteval auto get_test_content()
        {
            std::array<char, NumMessages * min_message_size> file{};
            auto timestamp{std::chrono::nanoseconds{imr::mold::downstream::market_pre}};

            for (auto i{0UZ}; i < NumMessages; ++i)
            {
                const std::span message{std::span{file}.subspan(i * min_message_size, min_message_size)};
                write_len_prefix(message);
                copy_timestamp(timestamp, message);
                timestamp += std::chrono::nanoseconds(TimestampIncrementNs);
            }
            return file;
        }

      private:
        static constexpr auto min_message_size{imr::mold::PacketBuilder::min_message_size};
        static constexpr imr::mold::types::LengthPrefix itch_min_body_size{
            min_message_size - sizeof(imr::mold::types::LengthPrefix)};

        static consteval void write_len_prefix(std::span<char> message)
        {
            const auto bytes{std::bit_cast<std::array<char, sizeof(imr::mold::types::LengthPrefix)>>(
                std::byteswap(itch_min_body_size))};
            std::ranges::copy(bytes, message.begin());
        }

        static consteval void copy_timestamp(std::chrono::nanoseconds timestamp, std::span<char> message)
        {
            auto count{std::byteswap(timestamp.count()) >> 16};
            const auto bytes{std::bit_cast<std::array<char, sizeof(std::uint64_t)>>(count)};

            static constexpr auto timestamp_offset{7UZ};
            static constexpr auto timestamp_size{6UZ};

            std::ranges::copy_n(bytes.data(), timestamp_size, message.begin() + timestamp_offset);
        }
    };
}
