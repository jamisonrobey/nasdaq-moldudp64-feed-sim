#pragma once

#include "imr/server.h"
#include <gtest/gtest.h>
#include <imr/mold/types.h>
#include <imr/mold/packet_builder.h>
#include <imr/mold/downstream/pacer.h>

#include <filesystem>
#include <fstream>

#include <span>
#include <chrono>
#include <algorithm>
#include <array>
#include <bit>

namespace test_common
{
    template <typename Derived>
    class TestFileFixture : public ::testing::Test
    {
      protected:
        static std::filesystem::path test_path()
        {
            return std::filesystem::path(TEST_DATA_DIR) / Derived::file_name();
        }

        static void SetUpTestSuite()
        {
            std::ofstream out(test_path(), std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(out) << "Failed to create temporary test file: " << test_path();
            const auto content{Derived::get_test_content()};
            out.write(content.data(), content.size());
        }

        static void TearDownTestSuite()
        {
            std::filesystem::remove(test_path());
        }

        // make similar servers with diff ports
        static std::unique_ptr<imr::Server> make_test_server(imr::Server::Config& config)
        {
            static std::atomic<std::uint16_t> downstream_port{3400};
            static std::atomic<std::uint16_t> retransmission_port{3500};

            config.mapped_itch_file_cfg.path = test_path();
            config.downstream_feed_config.port = downstream_port.fetch_add(1, std::memory_order_relaxed);
            config.retransmission_feed_config.port = retransmission_port.fetch_add(1, std::memory_order_relaxed);

            std::expected result{imr::make_server(config)};

            EXPECT_TRUE(result.has_value()) << std::format("Error during server construction: {}", result.error());

            return std::move(*result);
        }
    };

    template <std::size_t NumMessages, std::int64_t TimestampIncrementNs = 1>
    class ItchFileFixture : public TestFileFixture<ItchFileFixture<NumMessages, TimestampIncrementNs>>
    {
      public:
        static constexpr std::string_view file_name()
        {
            return "itch_test_file";
        }

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

        static constexpr imr::mold::types::LengthPrefix itch_min_body_size{min_message_size -
                                                                           sizeof(imr::mold::types::LengthPrefix)};

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
