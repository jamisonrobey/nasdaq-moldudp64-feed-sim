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
    /// MoldUDP64 retransmission server.
    class Feed
    {
      public:
        /// @ingroup config
        struct Config
        {
            /// Address to bind the retransmission socket to
            util::zstring_view address;
            /// Port to bind to. Pass 0 to let the OS assign an ephemeral port.
            std::uint16_t port;
        };
        /** Constructs and binds the retransmission socket.
         *
         * @param shutdown_fd fd polled alongside the socket; writing to this will cause `start()` to exit stopping the event loop.
         * Must be > 0 (0 is reserved for stdin, which epoll rejects with EPERM).
         *
         * @throws std::invalid_argument if shutdown_fd <= 0, or if cfg.address is not valid IPv4.
         *
         * @throws std::system_error if network resource creation / config fails
         */
        explicit Feed(const Config& cfg,
                      const PacketBuilder::Config& packet_builder_cfg,
                      std::span<const char> file,
                      const RetransmissionBuffer& retransmission_buffer,
                      int shutdown_fd);
        /** Runs the event loop, blocking until shutdown_fd becomes readable.
         *  Dispatches incoming client requests to handle_request().
         */
        void start();

      private:
        util::FileDescriptor socket_{[] { return socket(AF_INET, SOCK_DGRAM, 0); }};
        util::FileDescriptor epoll_fd_{[] { return epoll_create1(0); }};
        int shutdown_fd_;
        /**
         * @tparam N = 2, 1 for shutdown_fd_, 1 for request
         */
        std::array<epoll_event, 2> epoll_events_{};

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
