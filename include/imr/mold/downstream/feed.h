#pragma once

#include "imr/mold/downstream/heartbeat.h"
#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/downstream/pacer.h"

#include "imr/mold/types.h"
#include "imr/util/file_descriptor.h"
#include "imr/util/zstring_view.h"

#include <netinet/in.h>
#include <stop_token>
#include <sys/socket.h>

namespace imr::mold::downstream
{
    /** Replays a MoldUDP64 downstream feed over multicast.
     *
     *  Sends heartbeats on a fixed period while running, then
     *  end of session packets for `end_of_session_duration` once the file
     *  is exhausted or `start()`'s stop_token is triggered.
     */
    class Feed
    {
      public:
        struct Config
        {
            /// Multicast group to send to.
            util::zstring_view mcast_group;
            /// Multicast port.
            std::uint16_t port;
            /// Sets IP_MULTICAST_TTL.
            std::uint8_t ttl{1};
            /// Enable/Disable IP_MULTICAST_LOOP.
            bool loopback{false};
            /**
             Interface to send multicast packets from (IP_MULTICAST_IF).

             Set explicitly in multi nic environment.
            */
            in_addr egress_interface{.s_addr = htonl(INADDR_ANY)};
            /** Interval between heartbeat packets while the feed is running.
             *
             *  This is also the interval of the end of session packets.
             */
            std::chrono::nanoseconds heartbeat_period{std::chrono::seconds(1)};
            /// How long end of session should send packets
            std::chrono::nanoseconds end_of_session_duration{std::chrono::seconds(30)};
            /// Controls playback speed and pre-market message skipping.
            Pacer<std::chrono::steady_clock>::Config pacer_cfg{};
        };

        /** Constructs the feed ready to begin downstream on configured multicast group/port

         @throws std::invalid_argument if cfg.mcast_group is not a valid IPv4 address

         @throws std::system_error if socket creation / configuration fails
        */
        explicit Feed(const Config& cfg,
                      const PacketBuilder::Config& packet_builder_cfg,
                      std::span<const char> file,
                      RetransmissionBuffer& retransmission_buffer);

        /** Replays the file until EOF or `st` stopped, then send end of session packets for configured duration
         *
         *  Blocks until finished.
         */
        void start(std::stop_token st);

      private:
        util::FileDescriptor socket_{[] { return socket(AF_INET, SOCK_DGRAM, 0); }};
        sockaddr_in mcast_group_;
        msghdr send_hdr_{};

        std::span<const char> file_;
        std::size_t file_pos_{0};
        types::header::SequenceNumber sequence_number_{1};
        std::atomic<types::header::SequenceNumber> sent_sequence_number_{1};

        RetransmissionBuffer* retransmission_buffer_;

        Pacer<std::chrono::steady_clock> pacer_;
        PacketBuilder packet_builder_;
        Heartbeat heartbeat_;

        std::chrono::nanoseconds end_of_session_duration_;

        [[nodiscard]]
        sockaddr_in configure_socket(const Config& cfg) const;

        void build_packet();
        void send_packet() noexcept;

        void end_of_session(std::stop_token st);
    };
}
