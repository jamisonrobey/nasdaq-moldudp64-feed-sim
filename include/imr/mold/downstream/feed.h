#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/downstream/pacer.h"

#include "imr/util/file_descriptor.h"

#include <netinet/in.h>

namespace imr::mold::downstream
{
    class Feed
    {
      public:
        struct Config
        {
            // these probaly need to be std::string or const char* because passed to c function and need null terminator guarantee
            std::string_view mcast_group;
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
        void stop();
        // reset sequence_number and file position
        void reset(std::span<const char> new_file = {});

      private:
        util::FileDescriptor socket_;
        sockaddr_in mcast_addr_in_{};
        Pacer<std::chrono::steady_clock> pacer_;
        std::span<const char> file_;
        PacketBuilder packet_builder_;
        RetransmissionBuffer* retransmission_buffer_;

        SequenceNumber sequence_number_{1};
        std::size_t file_pos_{0};
        bool should_stop_{false};

        void setup_networking(const Config& cfg);
        std::chrono::nanoseconds fill_packet();
        void flush_packet(std::chrono::nanoseconds first_msg_timestamp);
        void handle_delay(std::chrono::nanoseconds first_msg_timestamp);
        void send(std::span<const char> msg);
    };
}
