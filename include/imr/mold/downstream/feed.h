#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/downstream/pacer.h"

#include "imr/util/file_descriptor.h"
#include "imr/util/zstring_view.h"

#include <netinet/in.h>
#include <sys/socket.h>

namespace imr::mold::downstream
{
    class Feed
    {
      public:
        struct Config
        {
            util::zstring_view mcast_group;
            std::uint16_t port;
            std::uint8_t ttl{1};
            bool loopback{false};
            Pacer<std::chrono::steady_clock>::Config pacer_cfg;
        };

        explicit Feed(const Config& cfg,
                      const PacketBuilder::Config& packet_builder_cfg,
                      std::span<const char> file,
                      RetransmissionBuffer& retransmission_buffer);

        // run until EOF
        void start();
        // stop early
        void stop() noexcept;

      private:
        util::FileDescriptor socket_;
        sockaddr_in dest_addr_{};
        msghdr send_hdr_{};

        std::span<const char> file_;
        std::size_t file_pos_{0};
        types::header::SequenceNumber sequence_number_{1};
        bool stop_requested_{false};

        Pacer<std::chrono::steady_clock> pacer_;
        PacketBuilder packet_builder_;
        RetransmissionBuffer* retransmission_buffer_;

        using Timestamp = std::chrono::nanoseconds;

        void configure_socket(const Config& cfg);
        Timestamp build_packet();
        void send_packet() noexcept;
        void apply_pacing(Timestamp first_msg_timestamp);
    };
}
