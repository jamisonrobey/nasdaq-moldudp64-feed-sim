#include <arpa/inet.h>
#include <bits/types/struct_timeval.h>
#include <concepts>
#include <gtest/gtest.h>
#include <memory>

#include "imr/mold/types.h"
#include "imr/util/file_descriptor.h"
#include "test_file_fixture.h"
#include "util/binary_io.h"

using namespace imr;

namespace
{
    struct MoldHeader
    {
        mold::types::header::Session session;
        mold::types::header::SequenceNumber sequence_number;
        mold::types::header::MessageCount message_count;
    };

    MoldHeader parse_header(std::span<const char> bytes, std::size_t& pos)
    {
        EXPECT_GE(bytes.size(), mold::types::header::length);

        return {
            .session = util::binary_io::read<mold::types::header::Session>(bytes, pos),
            .sequence_number = util::binary_io::read_be<mold::types::header::SequenceNumber>(bytes, pos),
            .message_count = util::binary_io::read_be<mold::types::header::MessageCount>(bytes, pos),
        };
    }

    template <typename Func>
    concept MessageCallback = std::invocable<Func, int, std::span<const char>>;

    template <MessageCallback Func>
    void foreach_message(std::span<const char> payload, mold::types::header::MessageCount count, Func&& callback)
    {
        auto pos{0UZ};

        for (auto i{0U}; i < count; ++i)
        {
            const auto msg_len{util::binary_io::read_be<mold::types::LengthPrefix>(payload, pos)};
            std::span<const char> msg{payload.subspan(pos, msg_len)};
            pos += msg_len;

            callback(i, msg);
        }
    }

    mold::types::header::MessageCount count_messages(std::span<const char> payload, mold::types::header::MessageCount count)
    {
        mold::types::header::MessageCount in_payload{0};

        foreach_message(payload, count, [&in_payload](int, std::span<const char>) {
            ++in_payload;
        });

        return in_payload;
    }

    constexpr std::chrono::milliseconds heartbeat_period(50);
    constexpr std::chrono::milliseconds end_of_session_duration(500);
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

    constexpr auto total_num_messages{1024};

    template <typename Func, typename... Args>
    void check_syscall(Func&& func, Args&&... args)
    {
        ASSERT_EQ(std::invoke(std::forward<Func>(func), std::forward<Args>(args)...), 0) << std::strerror(errno);
    }

    class E2ETestBase : public test_common::ItchFileFixture<total_num_messages>
    {
      protected:
        Server::Config test_config_{config};
        std::unique_ptr<imr::Server> server_;

        util::FileDescriptor mcast_socket_;

        static constexpr timeval timeout_tv_{
            .tv_sec = 1,
            .tv_usec = 0,
        };

        void TearDown() override
        {
            if (server_)
            {
                server_->stop();
                server_.reset();
            }
        }

        void create_multicast_socket()
        {
            mcast_socket_ = util::FileDescriptor(socket(AF_INET, SOCK_DGRAM, 0));
            constexpr auto sockopt_on{1};
            check_syscall(setsockopt, mcast_socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on));

            sockaddr_in local_addr{};
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(test_config_.downstream_feed_config.port);
            local_addr.sin_addr.s_addr = INADDR_ANY;

            check_syscall(bind, mcast_socket_.get(), reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr));

            ip_mreq mreq{};
            mreq.imr_multiaddr.s_addr = inet_addr(test_config_.downstream_feed_config.mcast_group.data());
            mreq.imr_interface.s_addr = INADDR_ANY;
            check_syscall(setsockopt, mcast_socket_.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

            check_syscall(setsockopt, mcast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout_tv_, sizeof(timeout_tv_));
        }
    };

    class E2ETestDownstream : public E2ETestBase
    {
      protected:
        void SetUp() override
        {
            server_ = make_test_server(test_config_);
            create_multicast_socket();
        }
    };

    class E2ETestRetransmission : public E2ETestBase
    {
      protected:
        util::FileDescriptor unicast_socket_;

        void SetUp() override
        {
            server_ = make_test_server(test_config_);
            create_multicast_socket();
            create_unicast_socket();
        }

        struct ParsedResponse
        {
            MoldHeader header;
            std::span<const char> payload;
        };

        void send_retransmission_request(mold::types::header::SequenceNumber seq_num, mold::types::header::MessageCount count)
        {
            static thread_local std::array<char, mold::types::header::length> request_buffer{};
            std::span request_span(request_buffer);

            util::binary_io::write_at(request_span, mold::types::header::session_offset, test_config_.packet_builder_cfg.session);
            util::binary_io::write_at_be(request_span, mold::types::header::sequence_number_offset, seq_num);
            util::binary_io::write_at_be(request_span, mold::types::header::message_count_offset, count);

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

        std::span<const char> get_expected_payload(mold::types::header::SequenceNumber sequence_number)
        {
            static const auto file_content{get_test_content()};

            constexpr auto min_message_size{imr::mold::PacketBuilder::min_message_size};
            const std::size_t offset{(sequence_number - 1) * min_message_size};

            return std::span<const char>(file_content.data() + offset + sizeof(mold::types::LengthPrefix),
                                         min_message_size - sizeof(mold::types::LengthPrefix));
        }

      private:
        void create_unicast_socket()
        {
            unicast_socket_ = util::FileDescriptor(socket(AF_INET, SOCK_DGRAM, 0));

            sockaddr_in local_addr{};
            local_addr.sin_family = AF_INET;
            // empheral loopback
            local_addr.sin_port = 0;
            local_addr.sin_addr.s_addr = inet_addr(test_config_.retransmission_feed_config.address.c_str());

            check_syscall(bind, unicast_socket_.get(), reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr));

            check_syscall(setsockopt, unicast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout_tv_, sizeof(timeout_tv_));
        }
    };
}

TEST_F(E2ETestDownstream, Downstream_LifecycleToShutdown)
{
    static constexpr auto expected_end_of_session_count{end_of_session_duration / heartbeat_period};

    mold::types::header::SequenceNumber expected_sequence{1};
    auto total_messages_received{0UZ};
    auto received_heartbeat{false};
    auto end_of_session_count{0UZ};

    server_->start();

    while (true)
    {
        static std::array<char, std::numeric_limits<std::uint16_t>::max()> recv_buffer{};

        ssize_t bytes_received{recv(mcast_socket_.get(), recv_buffer.data(), recv_buffer.size(), 0)};
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
            FAIL() << "Socket read failed: " << std::strerror(errno);
        }

        std::size_t pos{0};
        const MoldHeader header{parse_header(std::span(recv_buffer), pos)};

        EXPECT_EQ(std::string_view(header.session.data(), header.session.size()), test_config_.packet_builder_cfg.session);

        if (header.message_count == mold::types::header::heartbeat_msg_count)
        {
            EXPECT_EQ(header.sequence_number, expected_sequence);
            received_heartbeat = true;
        }
        else if (header.message_count == mold::types::header::end_of_session_msg_count)
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

            std::span<const char> payload(recv_buffer.data() + pos, bytes_received - pos);

            const auto messages_in_payload{count_messages(payload, header.message_count)};

            EXPECT_EQ(messages_in_payload, header.message_count);

            total_messages_received += header.message_count;
            expected_sequence += header.message_count;
        }
    }
}

TEST_F(E2ETestRetransmission, RetransmissionRequest_ValidRange)
{
    server_->start();

    std::array<char, MTU> buffer{};

    ssize_t bytes_received{0};
    MoldHeader mcast_header{};
    std::size_t pos{0};

    // wait till we get a non heartbeat packet
    while (true)
    {
        bytes_received = recv(mcast_socket_.get(), buffer.data(), buffer.size(), 0);
        ASSERT_GT(bytes_received, 0) << "Failed to receive downstream packet";

        pos = 0;
        mcast_header = parse_header(std::span(buffer), pos);

        if (mcast_header.message_count != mold::types::header::heartbeat_msg_count)
        {
            break;
        }
    }

    // copy messages to compare retransmission
    std::vector<std::vector<char>> received_messages;
    received_messages.reserve(mcast_header.message_count);

    foreach_message(std::span(buffer.data() + pos, bytes_received - pos),
                    mcast_header.message_count,
                    [&](std::uint16_t index, std::span<const char> msg) {
                        received_messages.emplace_back(msg.begin(), msg.end());
                    });

    send_retransmission_request(mcast_header.sequence_number, mcast_header.message_count);
    auto [header, payload] = receive_retransmission_response();

    EXPECT_EQ(std::string_view(header.session.data(), header.session.size()), test_config_.packet_builder_cfg.session);
    EXPECT_EQ(header.sequence_number, mcast_header.sequence_number);
    EXPECT_EQ(header.message_count, mcast_header.message_count);

    foreach_message(payload, header.message_count, [&](std::uint16_t index, std::span<const char> msg) {
        EXPECT_TRUE(std::ranges::equal(msg, received_messages[index]));
    });

    server_->stop();
}

TEST_F(E2ETestRetransmission, RetransmissionRequest_OutOfBounds)
{
    server_->start();

    // oob sequence
    constexpr mold::types::header::SequenceNumber oob_seq{total_num_messages + 1};

    send_retransmission_request(oob_seq, 1);

    std::array<char, MTU> rx_buffer{};
    ssize_t rx_bytes{recv(unicast_socket_.get(), rx_buffer.data(), rx_buffer.size(), 0)};

    // server ignores so we eventually timeout
    EXPECT_LT(rx_bytes, 0);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK)
        << "Expected EAGAIN/EWOULDBLOCK, got errno: " << errno;

    server_->stop();
}
