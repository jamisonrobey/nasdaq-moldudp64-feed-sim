#include <arpa/inet.h>
#include <gtest/gtest.h>

#include <imr/server.h>
#include <imr/mold/types.h>
#include "util/binary_io.h"

#include "server_test_fixture.h"
#include "itch_file_fixture.h"

using namespace imr;

namespace
{
    struct MoldHeader
    {
        mold::types::header::Session session;
        mold::types::header::SequenceNumber sequence_number;
        mold::types::header::MessageCount message_count;
    };

    MoldHeader parse_header(std::span<const char> bytes)
    {
        using namespace mold::types;

        EXPECT_GE(bytes.size(), header::length);

        return {
            .session = util::binary_io::read_at<header::Session>(bytes, header::session_offset),
            .sequence_number = util::binary_io::read_at_be<header::SequenceNumber>(bytes, header::sequence_number_offset),
            .message_count = util::binary_io::read_at_be<mold::types::header::MessageCount>(bytes, header::message_count_offset),
        };
    }

    // return message block as span from span that starts at header
    std::span<const char> message_block_span(std::span<const char> bytes)
    {
        assert(bytes.size() > mold::types::header::length);
        return std::span(bytes.subspan(mold::types::header::length, bytes.size() - mold::types::header::length));
    }

    template <typename Func>
    concept MessageCallback = std::invocable<Func, std::uint16_t, std::span<const char>>;

    template <MessageCallback Func>
    void for_each_message(std::span<const char> payload, mold::types::header::MessageCount count, Func&& callback)
    {
        std::size_t pos{0};
        for (std::uint16_t i{0}; i < count; ++i)
        {
            const auto len{util::binary_io::read_be<mold::types::LengthPrefix>(payload, pos)};
            callback(i, payload.subspan(pos, len));
            pos += len;
        }
    }

    mold::types::header::MessageCount count_messages(std::span<const char> payload, mold::types::header::MessageCount count)
    {
        mold::types::header::MessageCount n{0};
        for_each_message(payload, count, [&n](auto, auto) { ++n; });
        return n;
    }

    bool is_heartbeat(const MoldHeader& header)
    {
        return header.message_count == mold::types::header::heartbeat_msg_count;
    }

    bool is_end_of_session(const MoldHeader& header)
    {
        return header.message_count == mold::types::header::end_of_session_msg_count;
    }

    constexpr std::chrono::milliseconds heartbeat_period(50);
    constexpr std::chrono::milliseconds end_of_session_duration(500);
    constexpr auto MTU{1400};
    constexpr auto total_num_messages{1024};

    Server::Config base_config{
        .packet_builder_cfg = {.session = "SESSION001", .MTU = MTU},
        .downstream_feed_config = {
            .mcast_group = "239.0.0.1",
            .loopback = true,
            .egress_interface = {.s_addr = htonl(INADDR_LOOPBACK)},
            .heartbeat_period = heartbeat_period,
            .end_of_session_duration = end_of_session_duration,
            .pacer_cfg = {.skip_before = std::chrono::nanoseconds(0)},
        },
        .retransmission_feed_config = {.address = "127.0.0.1"},
    };

    template <typename Func, typename... Args>
    void require_syscall(Func&& func, Args&&... args)
    {
        ASSERT_EQ(std::invoke(std::forward<Func>(func), std::forward<Args>(args)...), 0) << std::strerror(errno);
    }

    class E2ETestBase : public test_common::ServerTestFixture<test_common::ItchFileFixture<total_num_messages>>
    {
      protected:
        Server::Config cfg_{base_config};
        std::unique_ptr<Server> server_;
        util::FileDescriptor mcast_socket_;

        static constexpr timeval recv_timeout_{.tv_sec = 1, .tv_usec = 0};

        void TearDown() override
        {
            if (server_)
            {
                server_->stop();
                server_.reset();
            }
        }

        // Must run BEFORE make_test_server() -- we bind to port 0, hold this
        // socket open for the fixture's lifetime, and feed the OS-assigned port
        // into cfg_ so the server knows where to send. Holding the socket open
        // (rather than closing after reading getsockname) is what actually
        // prevents port collisions under parallel ctest.
        void create_multicast_socket()
        {
            mcast_socket_ = util::FileDescriptor(socket(AF_INET, SOCK_DGRAM, 0));
            constexpr int sockopt_on{1};
            require_syscall(setsockopt, mcast_socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on));

            sockaddr_in addr{.sin_family = AF_INET, .sin_port = 0, .sin_addr = {.s_addr = INADDR_ANY}};
            require_syscall(bind, mcast_socket_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

            socklen_t len{sizeof(addr)};
            require_syscall(getsockname, mcast_socket_.get(), reinterpret_cast<sockaddr*>(&addr), &len);
            cfg_.downstream_feed_config.port = ntohs(addr.sin_port);

            ip_mreq mreq{
                .imr_multiaddr = {.s_addr = inet_addr(cfg_.downstream_feed_config.mcast_group.data())},
                .imr_interface = cfg_.downstream_feed_config.egress_interface,
            };
            require_syscall(setsockopt, mcast_socket_.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
            require_syscall(setsockopt, mcast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_, sizeof(recv_timeout_));
        }
    };

    class E2ETestDownstream : public E2ETestBase
    {
      protected:
        void SetUp() override
        {
            create_multicast_socket();
            server_ = make_test_server(cfg_);
        }
    };

    class E2ETestRetransmission : public E2ETestBase
    {
      protected:
        util::FileDescriptor unicast_socket_;

        void SetUp() override
        {
            create_multicast_socket();
            create_unicast_socket();
            server_ = make_test_server(cfg_);
        }

        void send_retransmission_request(mold::types::header::SequenceNumber seq, mold::types::header::MessageCount count)
        {
            std::array<char, mold::types::header::length> buf{};

            std::span span(buf);
            util::binary_io::write_at(span, mold::types::header::session_offset, cfg_.packet_builder_cfg.session);
            util::binary_io::write_at_be(span, mold::types::header::sequence_number_offset, seq);
            util::binary_io::write_at_be(span, mold::types::header::message_count_offset, count);

            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port = htons(cfg_.retransmission_feed_config.port);
            dest.sin_addr = {.s_addr = inet_addr(cfg_.retransmission_feed_config.address.c_str())};

            ASSERT_EQ(sendto(unicast_socket_.get(),
                             buf.data(),
                             buf.size(),
                             0,
                             reinterpret_cast<const sockaddr*>(&dest),
                             sizeof(dest)),
                      static_cast<ssize_t>(buf.size()))
                << "Failed to send retransmission request";
        }

        struct Response
        {
            MoldHeader header;
            std::span<const char> payload;
        };

        Response receive_retransmission_response()
        {
            static thread_local std::array<char, MTU> recv_buff{};

            const ssize_t bytes_recv{recv(unicast_socket_.get(), recv_buff.data(), recv_buff.size(), 0)};
            if (bytes_recv < 0)
            {
                throw std::system_error(errno, std::system_category(), "recv failed on unicast socket");
            }

            const std::span<const char> recv_buff_span(recv_buff.data(), bytes_recv);
            const MoldHeader header{parse_header(recv_buff_span)};

            return {
                header,
                message_block_span(recv_buff_span),
            };
        }

      private:
        void create_unicast_socket()
        {
            unicast_socket_ = util::FileDescriptor(socket(AF_INET, SOCK_DGRAM, 0));

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = 0;
            addr.sin_addr = {.s_addr = inet_addr(cfg_.retransmission_feed_config.address.c_str())};

            require_syscall(bind, unicast_socket_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            require_syscall(setsockopt, unicast_socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &recv_timeout_, sizeof(recv_timeout_));
        }
    };
}

TEST_F(E2ETestDownstream, LifeCycleToShutdown)
{
    constexpr auto expected_eos_count{end_of_session_duration / heartbeat_period};

    std::optional<mold::types::header::SequenceNumber> next_expected_seq;
    std::size_t eos_count{0};
    bool got_heartbeat{false};

    static std::array<char, MTU> recv_buf{};

    server_->start();

    while (true)
    {
        const ssize_t bytes_recv{recv(mcast_socket_.get(), recv_buf.data(), sizeof(recv_buf), 0)};

        if (bytes_recv < 0)
        {
            ASSERT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK) << "recv failed: " << std::strerror(errno);
            EXPECT_EQ(eos_count, expected_eos_count) << "Timed out before receiving all end-of-session packets";
            break;
        }

        std::span recv_buff_span(recv_buf.data(), bytes_recv);

        const MoldHeader header{parse_header(recv_buff_span)};
        EXPECT_EQ(std::string_view(header.session.data(), header.session.size()), cfg_.packet_builder_cfg.session);

        if (is_heartbeat(header))
        {
            got_heartbeat = true;
        }
        else if (is_end_of_session(header))
        {
            ASSERT_TRUE(next_expected_seq.has_value()) << "Got to end of session without ever setting next_expected_seq";
            EXPECT_EQ(header.sequence_number, *next_expected_seq);

            if (++eos_count >= expected_eos_count)
            {
                break;
            }
        }
        else
        {
            if (!next_expected_seq.has_value())
            {
                next_expected_seq = header.sequence_number;
            }
            else
            {
                EXPECT_EQ(header.sequence_number, *next_expected_seq);
            }

            std::span message_block{message_block_span(recv_buff_span)};

            EXPECT_EQ(count_messages(message_block, header.message_count), header.message_count);
            *next_expected_seq += header.message_count;
        }
    }
}

TEST_F(E2ETestRetransmission, ValidRange)
{
    server_->start();

    std::array<char, MTU> recv_buffer{};

    ssize_t bytes_recv{0};
    MoldHeader mcast_header{};

    // wait till we receive non heartbeat packet
    while (true)
    {
        bytes_recv = recv(mcast_socket_.get(), recv_buffer.data(), recv_buffer.size(), 0);
        ASSERT_GT(bytes_recv, 0) << "Failed to receive downstream packet";

        std::span recv_buf_span(recv_buffer.data(), bytes_recv);
        mcast_header = parse_header(recv_buf_span);
        if (!is_heartbeat(mcast_header))
        {
            break;
        }
    }

    std::vector<std::vector<char>> original_messages;
    original_messages.reserve(mcast_header.message_count);
    for_each_message(message_block_span(std::span(recv_buffer.data(), bytes_recv)),
                     mcast_header.message_count,
                     [&](auto, std::span<const char> msg) {
                         original_messages.emplace_back(msg.begin(), msg.end());
                     });

    send_retransmission_request(mcast_header.sequence_number, mcast_header.message_count);
    auto [retrans_header, retrans_payload] = receive_retransmission_response();

    EXPECT_EQ(std::string_view(retrans_header.session.data(), retrans_header.session.size()), cfg_.packet_builder_cfg.session);
    EXPECT_EQ(retrans_header.sequence_number, mcast_header.sequence_number);
    EXPECT_EQ(retrans_header.message_count, mcast_header.message_count);

    for_each_message(retrans_payload, retrans_header.message_count, [&](std::uint16_t i, std::span<const char> msg) {
        EXPECT_TRUE(std::ranges::equal(msg, original_messages[i]));
    });

    server_->stop();
}

TEST_F(E2ETestRetransmission, OutOfBounds)
{
    server_->start();

    constexpr mold::types::header::SequenceNumber oob_sequence{total_num_messages + 1};
    send_retransmission_request(oob_sequence, 1);

    std::array<char, MTU> recv_buffer{};

    const ssize_t bytes_recv{recv(unicast_socket_.get(), recv_buffer.data(), recv_buffer.size(), 0)};

    // server will ignore oob so we just wait for timeout
    EXPECT_LT(bytes_recv, 0);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK) << "Expected timeout, got errno: " << errno;

    server_->stop();
}
