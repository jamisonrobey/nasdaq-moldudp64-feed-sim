#include "imr/mold/types.h"
#include "imr/util/file_descriptor.h"
#include "util/binary_io.h"
#include <arpa/inet.h>
#include <chrono>

#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <sys/socket.h>
#include <test_file_fixture.h>

namespace
{
    constexpr std::chrono::milliseconds heartbeat_period(50);
    constexpr std::chrono::milliseconds end_of_session_duration(500);
    constexpr auto expected_end_of_session_count{end_of_session_duration.count() / heartbeat_period.count()};
    constexpr auto MTU{1400};

    imr::Server::Config config{
        .packet_builder_cfg =
            {
                .session = "SESSION001",
                .MTU = MTU,
            },
        .downstream_feed_config =
            {
                .mcast_group = "239.0.0.1",
                .loopback = true,
                .heartbeat_period = heartbeat_period,
                .end_of_session_duration = end_of_session_duration,
                .pacer_cfg =
                    {
                        .skip_before = std::chrono::nanoseconds(0),
                    },
            },
        .retransmission_feed_config =
            {
                .address = "127.0.0.1",
            },
    };

    constexpr auto num_messages{1024UZ};

    using namespace imr;
    using namespace imr::mold::types;

    struct MoldHeader
    {
        header::Session session;
        header::SequenceNumber sequence_number;
        header::MessageCount message_count;
    };

    MoldHeader parse_header(std::span<const char> bytes, std::size_t& pos)
    {
        EXPECT_GE(bytes.size(), header::length);

        return {
            .session = util::binary_io::read<header::Session>(bytes, pos),
            .sequence_number = util::binary_io::read_be<header::SequenceNumber>(bytes, pos),
            .message_count = util::binary_io::read_be<header::MessageCount>(bytes, pos),
        };
    }
}

class E2ETest : public test_common::ItchFileFixture<num_messages>
{
  protected:
    imr::Server::Config test_config_{config};
    std::unique_ptr<imr::Server> server_;

    util::FileDescriptor mcast_socket_;
    util::FileDescriptor unicast_socket_;

    void SetUp() override
    {
        server_ = make_test_server(test_config_);

        create_multicast_socket();
        create_unicast_socket();
    }

    void TearDown() override
    {
        if (server_)
        {
            server_->stop();
            server_.reset();
        }
    }

    void send_retransmission_request(header::SequenceNumber seq_num, header::MessageCount count)
    {
        static thread_local std::array<char, header::length> request_buffer{};
        std::span request_span(request_buffer);

        util::binary_io::write_at(request_span, header::session_offset, test_config_.packet_builder_cfg.session);
        util::binary_io::write_at_be(request_span, header::sequence_number_offset, seq_num);
        util::binary_io::write_at_be(request_span, header::message_count_offset, count);

        sockaddr_in dest_addr{};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(test_config_.retransmission_feed_config.port);
        dest_addr.sin_addr.s_addr = inet_addr(test_config_.retransmission_feed_config.address.c_str());

        ASSERT_EQ(sendto(unicast_socket_.get(),
                         request_buffer.data(),
                         request_buffer.size(),
                         0,
                         reinterpret_cast<const sockaddr*>(&dest_addr),
                         sizeof(dest_addr)),
                  static_cast<ssize_t>(request_buffer.size()))
            << "Failed to send retransmission request";
    }

    struct ParsedResponse
    {
        MoldHeader header;
        std::span<const char> payload;
    };

    ParsedResponse receive_retransmission_response()
    {
        static thread_local std::array<char, MTU> recv_buffer{};

        ssize_t bytes_received{recv(unicast_socket_.get(), recv_buffer.data(), recv_buffer.size(), 0)};

        if (bytes_received < 0)
        {
            throw std::system_error(errno, std::system_category(), "recv failed on unicast socket");
        }

        std::size_t pos{0};
        const MoldHeader header{parse_header(std::span(recv_buffer), pos)};

        std::span<const char> payload(recv_buffer.data() + pos, bytes_received - pos);

        return {
            header,
            payload,
        };
    }

    template <typename Func>
    void for_each_message(std::span<const char> payload, header::MessageCount count, Func&& callback)
    {
        std::size_t pos{0};
        for (std::uint16_t i = 0; i < count; ++i)
        {
            const auto msg_len{util::binary_io::read_be<LengthPrefix>(payload, pos)};
            std::span<const char> msg{payload.subspan(pos, msg_len)};
            pos += msg_len;

            callback(i, msg);
        }
    }

    std::span<const char> get_expected_payload(header::SequenceNumber sequence_number)
    {
        static const auto file_content{get_test_content()};

        constexpr auto min_message_size{imr::mold::PacketBuilder::min_message_size};
        const std::size_t offset{(sequence_number - 1) * min_message_size};

        return std::span<const char>(file_content.data() + offset + sizeof(LengthPrefix),
                                     min_message_size - sizeof(LengthPrefix));
    }

  private:
    void create_multicast_socket()
    {
        mcast_socket_ = util::FileDescriptor(socket(AF_INET, SOCK_DGRAM, 0));

        int sockopt_on{1};
        ASSERT_EQ(setsockopt(mcast_socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)), 0)
            << "Failed to set SO_REUSEADDR on multicast socket";

        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(test_config_.downstream_feed_config.port);
        local_addr.sin_addr.s_addr = INADDR_ANY;

        ASSERT_EQ(bind(mcast_socket_.get(), reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)), 0)
            << "Failed to bind multicast socket to port " << test_config_.downstream_feed_config.port;

        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(test_config_.downstream_feed_config.mcast_group.data());
        mreq.imr_interface.s_addr = INADDR_ANY;

        ASSERT_EQ(setsockopt(mcast_socket_.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)), 0)
            << "Failed to join multicast group " << test_config_.downstream_feed_config.mcast_group.c_str();
    }

    void create_unicast_socket()
    {
        unicast_socket_ = util::FileDescriptor(socket(AF_INET, SOCK_DGRAM, 0));

        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = 0;
        local_addr.sin_addr.s_addr = inet_addr(test_config_.retransmission_feed_config.address.c_str());

        ASSERT_EQ(bind(unicast_socket_.get(), reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)), 0)
            << "Failed to bind unicast socket";
    }
};

TEST_F(E2ETest, Downstream_LifecycleToShutdown)
{
    timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    ASSERT_EQ(setsockopt(mcast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0) << std::strerror(errno);

    header::SequenceNumber expected_sequence{1};
    auto total_messages_received{0UZ};
    auto received_heartbeat{false};
    auto end_of_session_count{0UZ};

    std::array<char, std::numeric_limits<std::uint16_t>::max()> buffer{};

    server_->start();

    while (true)
    {
        ssize_t bytes_received{recv(mcast_socket_.get(), buffer.data(), buffer.size(), 0)};
        if (bytes_received < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                if (end_of_session_count != expected_end_of_session_count)
                {
                    FAIL() << "Timed out waiting for downstream packet";
                }
                break;
            }
            FAIL() << "Socket read failed with errno: " << errno;
        }

        std::size_t pos{0};
        const MoldHeader header{parse_header(std::span(buffer), pos)};

        EXPECT_EQ(std::string_view(header.session.data(), header.session.size()), test_config_.packet_builder_cfg.session);

        if (header.message_count == header::heartbeat_msg_count)
        {
            EXPECT_EQ(header.sequence_number, expected_sequence);
            received_heartbeat = true;
        }
        else if (header.message_count == header::end_of_session_msg_count)
        {
            EXPECT_EQ(header.sequence_number, expected_sequence);
            ++end_of_session_count;

            if (end_of_session_count >= expected_end_of_session_count)
            {
                break;
            }
        }
        else
        {
            EXPECT_EQ(header.sequence_number, expected_sequence);

            std::span<const char> payload(buffer.data() + pos, bytes_received - pos);

            for_each_message(payload, header.message_count, [&](std::uint16_t, std::span<const char>) {});

            total_messages_received += header.message_count;
            expected_sequence += header.message_count;
        }
    }

    EXPECT_EQ(total_messages_received, num_messages);
    EXPECT_TRUE(received_heartbeat);
    EXPECT_GE(end_of_session_count, expected_end_of_session_count);

    server_->wait_for_downstream();
}

TEST_F(E2ETest, RetransmissionRequest_ValidRange)
{
    // Set a 2-second timeout on both sockets
    timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    ASSERT_EQ(setsockopt(mcast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);
    ASSERT_EQ(setsockopt(unicast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);

    server_->start();

    std::array<char, std::numeric_limits<std::uint16_t>::max()> buffer{};
    ssize_t bytes_received{0};
    MoldHeader mcast_header{};
    std::size_t pos{0};

    while (true)
    {
        bytes_received = recv(mcast_socket_.get(), buffer.data(), buffer.size(), 0);
        ASSERT_GT(bytes_received, 0) << "Failed to receive live multicast packet";

        pos = 0;
        mcast_header = parse_header(std::span(buffer), pos);

        if (mcast_header.message_count > 0 && mcast_header.message_count != header::end_of_session_msg_count)
        {
            break;
        }
    }

    // Store the multicast message payloads so we can verify the retransmitted copies against them
    std::vector<std::vector<char>> received_messages;
    received_messages.reserve(mcast_header.message_count);

    std::span<const char> mcast_payload(buffer.data() + pos, bytes_received - pos);
    for_each_message(mcast_payload, mcast_header.message_count, [&](std::uint16_t, std::span<const char> msg) {
        received_messages.emplace_back(msg.begin(), msg.end());
    });

    // 2. Request a retransmission of the exact range we just received
    send_retransmission_request(mcast_header.sequence_number, mcast_header.message_count);

    // 3. Receive the response on our unicast socket
    auto [header, payload] = receive_retransmission_response();

    // 4. Verify headers and zero-copy message payloads
    EXPECT_EQ(std::string_view(header.session.data(), header.session.size()), test_config_.packet_builder_cfg.session);
    EXPECT_EQ(header.sequence_number, mcast_header.sequence_number);
    EXPECT_EQ(header.message_count, mcast_header.message_count);

    for_each_message(payload, header.message_count, [&](std::uint16_t index, std::span<const char> msg) {
        EXPECT_TRUE(std::ranges::equal(msg, received_messages[index]));
    });

    server_->stop();
}

TEST_F(E2ETest, RetransmissionRequest_OutOfBounds)
{
    // Use a short 200ms timeout for the OOB check to keep tests fast
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    ASSERT_EQ(setsockopt(mcast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);
    ASSERT_EQ(setsockopt(unicast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), 0);

    server_->start();

    // 1. Wait for at least one downstream packet to verify the server is active
    std::array<char, std::numeric_limits<std::uint16_t>::max()> buffer{};
    ssize_t bytes_received{recv(mcast_socket_.get(), buffer.data(), buffer.size(), 0)};
    ASSERT_GT(bytes_received, 0);

    // 2. Request sequence numbers way beyond the file content (e.g. 5000)
    constexpr header::SequenceNumber oob_seq{5000};
    constexpr header::MessageCount oob_count{5};

    send_retransmission_request(oob_seq, oob_count);

    // 3. Confirm that the server ignores the out-of-bounds request (causing a timeout)
    std::array<char, MTU> rx_buffer{};
    ssize_t rx_bytes = recv(unicast_socket_.get(), rx_buffer.data(), rx_buffer.size(), 0);

    EXPECT_LT(rx_bytes, 0);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK)
        << "Expected EAGAIN/EWOULDBLOCK, got errno: " << errno;

    server_->stop();
}
