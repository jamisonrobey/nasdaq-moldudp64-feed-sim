#pragma once

#include "imr/mold/retransmission_buffer.h"
#include "imr/util/file_descriptor.h"
#include "imr/util/zstring_view.h"
#include "imr/mold/packet_builder.h"

#include <array>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>

namespace imr::mold::retransmission
{
    class Feed
    {
      public:
        struct Config
        {
            // valid ip address like 127.0.0.1 or throws system_error if inet_pton fails to set address
            util::zstring_view address;
            // valid port or throws system error when bind fails (check) chose 0 to let os assign port
            std::uint16_t port;
        };

        explicit Feed(const Config& cfg,
                      const PacketBuilder::Config& packet_builder_cfg,
                      std::span<const char> file,
                      const RetransmissionBuffer& retransmission_buffer,
                      int shutdown_fd);

        void start();

      private:
        util::FileDescriptor socket_{[] { return socket(AF_INET, SOCK_DGRAM, 0); }};
        util::FileDescriptor epoll_fd_{[] { return epoll_create1(0); }};
        int shutdown_fd_;
        // so we dont have to static_cast .size() each time when calling epoll_wait()
        static constexpr int num_epoll_events{2};
        std::array<epoll_event, num_epoll_events> epoll_events_{};
        std::array<char, types::header::length> recv_buffer_{};

        PacketBuilder packet_builder_;
        std::span<const char> file_;

        const RetransmissionBuffer* retransmission_buffer_;

        void handle_request(int client_fd);

        struct RequestContext
        {
            sockaddr_in client_address;
            types::header::SequenceNumber starting_sequence;
            types::header::MessageCount msg_count;
            std::size_t file_position_for_retransmission;
        };

        std::optional<RequestContext> parse_request(sockaddr_in&& client_addr);

        void build_packet(const RequestContext& ctx);
        void send_packet(const sockaddr_in& client_address);

        void configure_socket(const Config& cfg);
    };
}
