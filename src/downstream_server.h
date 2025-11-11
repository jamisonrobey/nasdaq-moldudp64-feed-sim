#ifndef DOWNSTREAM_SERVER_H
#define DOWNSTREAM_SERVER_H

#include "jamutils/FD.h"
#include "retransmission_buffer.h"
#include "packet_builder.h"
#include "temporal_pacer.h"
#include "nasdaq.h"
#include <netinet/in.h>
#include <cstdint>
#include <chrono>

class DownstreamServer
{
  public:
    DownstreamServer(std::string_view session,
                     std::span<const std::byte> file,
                     RetransmissionBuffer* retrans_buffer,
                     std::string_view mcast_group,
                     std::uint16_t port,
                     std::uint8_t mcast_ttl,
                     bool loopback,
                     double replay_speed,
                     Nasdaq::MarketPhase start_phase);

    void run();

  private:
    PacketBuilder packet_builder_;
    std::span<const std::byte> file_;
    RetransmissionBuffer* retrans_buffer_;
    TemporalPacer<> pacer_;

    jam_utils::FD sock_;
    sockaddr_in dest_addr_{};

    void handle_replay_timing(const std::chrono::nanoseconds& packet_timestamp);
    void send_packet();
    void end_of_session(std::uint64_t final_seq_num);
};

#endif
