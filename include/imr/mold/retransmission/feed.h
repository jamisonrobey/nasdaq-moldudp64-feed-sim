#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/types.h"
#include "imr/util/file_descriptor.h"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include <string_view>
#include <cstdint>

namespace imr::mold::retransmission
{
    class Feed
    {
      public:
        struct Config
        {
            std::string_view address;
            std::uint16_t port;
            // epoll_wait() take int size not size type
            int epoll_max_events{1024};
        };

        Feed(const Config& cfg,
             int shutdown_fd,
             const PacketBuilder::Config& packet_builder_cfg,
             std::span<const char> file,
             RetransmissionBuffer& retransmission_buffer);

        void start();

      private:
        util::FileDescriptor socket_{socket(AF_INET, SOCK_DGRAM, 0)};
        util::FileDescriptor epoll_fd_{epoll_create1(0)};
        std::vector<epoll_event> epoll_events_;

        sockaddr_in server_addr_in{};

        int shutdown_fd_;

        PacketBuilder packet_builder_;
        std::span<const char> file_;

        RetransmissionBuffer* retransmission_buffer_;

        bool running_{false};

        void handle_events();
        void process_request(int client_fd);

        struct RequestContext
        {
            sockaddr_in client_address;
            SequenceNumber starting_sequence;
            MessageCount msg_count;
            std::size_t file_position_for_retransmission;
        };
        std::optional<RequestContext> parse_request(int client_fd);

        void fill_packet(const RequestContext& ctx);
        void flush_packet();

        void setup_networking(const Config& cfg);
    };
};
