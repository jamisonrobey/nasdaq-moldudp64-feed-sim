#pragma once

#include "imr/mold/downstream/heartbeat.h"
#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/downstream/pacer.h"

#include "imr/util/file_descriptor.h"
#include "imr/util/zstring_view.h"

#include <netinet/in.h>
#include <stop_token>
#include <sys/socket.h>

namespace imr::mold::downstream
{
    class Feed
    {
      public:
        struct Config
        {
            // required
            util::zstring_view mcast_group;
            std::uint16_t port;
            // socket options
            std::uint8_t ttl{1};
            bool loopback{false};
            // timing
            std::chrono::nanoseconds heartbeat_period{std::chrono::seconds(1)};
            std::chrono::nanoseconds end_of_session_duration{std::chrono::seconds(30)};
            // pacing
            Pacer<std::chrono::steady_clock>::Config pacer_cfg{};
        };

        explicit Feed(const Config& cfg,
                      const PacketBuilder::Config& packet_builder_cfg,
                      std::span<const char> file,
                      RetransmissionBuffer& retransmission_buffer);

        // block until EOF
        void start(std::stop_token st);

      private:
        using Timestamp = std::chrono::nanoseconds;

        util::FileDescriptor socket_;
        sockaddr_in mcast_group_;
        msghdr send_hdr_{};

        std::span<const char> file_;
        std::size_t file_pos_{0};
        std::atomic<types::header::SequenceNumber> sequence_number_{1};

        RetransmissionBuffer* retransmission_buffer_;

        Pacer<std::chrono::steady_clock> pacer_;
        PacketBuilder packet_builder_;
        Heartbeat heartbeat_;

        std::chrono::nanoseconds end_of_session_duration_;

        [[nodiscard]]
        sockaddr_in configure_socket(const Config& cfg);

        [[nodiscard]]
        std::optional<Timestamp> build_packet();
        void send_packet() noexcept;
        void apply_pacing(Timestamp first_msg_timestamp);

        void end_of_session(std::stop_token st);
    };
}
