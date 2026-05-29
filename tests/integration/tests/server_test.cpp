#include <gtest/gtest.h>

#include "imr/mold/packet_builder.h"
#include "imr/mold/types.h"

#include <bit>
#include <imr/server.h>
#include <cstring>
#include <chrono>
#include <array>
#include <cstddef>
#include <fstream>

using namespace imr;

namespace
{
    consteval void write_len_prefix(std::span<char> message)
    {
        const auto bytes{std::bit_cast<std::array<char, sizeof(mold::types::LengthPrefix)>>(std::byteswap(mold::PacketBuilder::min_message_size_totalview_itch))};
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

    constexpr auto min_message_size{mold::PacketBuilder::min_message_size_totalview_itch + sizeof(mold::types::LengthPrefix)};

    template <std::size_t N>
    consteval std::array<char, N * min_message_size> create_test_file()
    {
        static constexpr std::chrono::milliseconds timestamp_increment{10};

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
class ServerTest : public ::testing::Test
{
  protected:
    static constexpr std::string_view test_path{TEST_DATA_DIR "/test_file"};

    static void SetUpTestSuite()
    {
        static constexpr std::array test_file{create_test_file<1024>()};

        std::ofstream out(std::filesystem::path(test_path), std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out) << "Failed to create test file";

        out.write(test_file.data(), test_file.size());
    }

    static void TearDownTestSuite()
    {
        std::filesystem::remove(test_path);
    }

    static std::unique_ptr<imr::Server> make_test_server(std::uint16_t downstream_port, std::uint16_t retransmission_port)
    {
        std::expected result{imr::make_server(imr::Server::Config{
            .mapped_itch_file_cfg = {
                .path = test_path,
            },
            .packet_builder_cfg = {
                .session = "SESSION001",
            },
            .downstream_feed_config = {
                .mcast_group = "239.0.0.1",
                .port = downstream_port,
                .pacer_cfg = {
                    .skip_before = imr::mold::downstream::market_open,
                },
            },
            .retransmission_feed_config = {
                .address = "127.0.0.1",
                .port = retransmission_port,
            },
        })};

        EXPECT_TRUE(result.has_value()) << std::format("Error during server construction: {}", result.error());

        return std::move(*result);
    }
};

TEST_F(ServerTest, Start_RunToEOF)
{
    const std::unique_ptr<imr::Server> server{make_test_server(3400, 3500)};

    server->start();
    server->wait_for_downstream();
}

TEST_F(ServerTest, Stop_WhileRunning)
{

    const std::unique_ptr<imr::Server> server{make_test_server(4400, 4500)};

    server->start();
    server->stop();
}

TEST_F(ServerTest, Stop_AfterEOF)
{

    const std::unique_ptr<imr::Server> server{make_test_server(5400, 5500)};

    server->start();
    server->wait_for_downstream();
    server->stop();
}
