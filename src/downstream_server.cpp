#include "downstream_server.h"
#include "itch.h"
#include "mold_udp_64.h"
#include "nasdaq.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <thread>
#include <print>
#include <iostream>
#include <cassert>

DownstreamServer::DownstreamServer(std::string_view session,
                                   std::span<const std::byte> file,
                                   RetransmissionBuffer* retrans_buffer,
                                   std::string_view mcast_group,
                                   std::uint16_t port,
                                   std::uint8_t mcast_ttl,
                                   bool loopback,
                                   double replay_speed,
                                   Nasdaq::MarketPhase start_phase)

    : packet_builder_{session},
      file_{file},
      retrans_buffer_{retrans_buffer},
      pacer_{replay_speed, Nasdaq::market_phase_to_timestamp(start_phase), std::chrono::steady_clock::now()},
      sock_{socket(AF_INET, SOCK_DGRAM, 0)}
{
    constexpr auto sockopt_on{1};
    if (setsockopt(sock_.fd(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0 ||
        setsockopt(sock_.fd(), IPPROTO_IP, IP_MULTICAST_TTL, &mcast_ttl, sizeof(mcast_ttl)) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    const int loopback_opt = loopback ? 1 : 0; // opt needs to be int* (bool* is ub)
    if (setsockopt(sock_.fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &loopback_opt, sizeof(loopback_opt)) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    dest_addr_.sin_family = AF_INET;
    dest_addr_.sin_port = htons(port);

    if (const auto ret{inet_pton(AF_INET, mcast_group.data(), &dest_addr_.sin_addr)}; ret == 0)
    {
        throw std::invalid_argument(std::format("invalid ip format for downstream group {}", mcast_group));
    }
    else if (ret < 0)
    {
        throw std::system_error(errno, std::system_category());
    }
}

void DownstreamServer::run()
{
    std::uint64_t seq_num{1};
    std::size_t file_pos{0};

    while (file_pos < file_.size())
    {
        packet_builder_.reset(seq_num);
        std::optional<std::chrono::nanoseconds> packet_timestamp{std::nullopt};

        while (const auto message{Itch::seek_next_message(file_, file_pos)})
        {
            if (!packet_builder_.try_add_message(*message))
            {
                break;
            }
#ifndef NDEBUG
            static std::uint64_t last_seq_num{};
            assert(last_seq_num < seq_num && "downstream sequence number not monotonically increasing");
            last_seq_num = seq_num;
#endif

            if (!packet_timestamp.has_value())
            {
		packet_timestamp = Itch::extract_timestamp(message->subspan(Itch::timestamp_offset, Itch::timestamp_size));
            }

            file_pos += message->size();
            retrans_buffer_->push({.seq_num = seq_num, .file_pos = file_pos});
            ++seq_num;
        }

        if (const auto delay{pacer_.get_delay(*packet_timestamp)})
        {
#ifndef DEBUG_NO_SLEEP
            std::this_thread::sleep_for(*delay);
#endif

#ifndef DEBUG_NO_NETWORK
            send_packet();
#endif
        }
    }

#ifndef DEBUG_NO_NETWORK
    end_of_session(seq_num);
#endif
}
void DownstreamServer::send_packet()
{
    packet_builder_.finalize();
    if (const auto bytes_sent{sendto(sock_.fd(),
                                     packet_builder_.cbegin(),
                                     packet_builder_.size(),
                                     0,
                                     reinterpret_cast<sockaddr*>(&dest_addr_),
                                     sizeof(dest_addr_))};
        bytes_sent < 0)
    {
        std::perror("sendto");
    }
    else if (static_cast<std::size_t>(bytes_sent) != packet_builder_.size())
    {
        std::println(std::cerr, "sendto: only sent {} of {} bytes", bytes_sent, packet_builder_.size());
    }
}

void DownstreamServer::end_of_session(std::uint64_t final_seq_num)
{
    MoldUDP64::DownstreamHeader eos_header{packet_builder_.session()};
    eos_header.msg_count = MoldUDP64::end_of_session_flag;
    eos_header.sequence_num = htobe64(final_seq_num);

    std::array<std::byte, sizeof(MoldUDP64::DownstreamHeader)> eos_packet{};
    std::memcpy(eos_packet.begin(), &eos_header, sizeof(eos_header));

    for (std::size_t i = 0; i < MoldUDP64::end_of_session_duration.count(); ++i)
    {
        if (const auto bytes_sent{sendto(sock_.fd(),
                                         eos_packet.data(),
                                         eos_packet.size(),
                                         0,
                                         reinterpret_cast<sockaddr*>(&dest_addr_),
                                         sizeof(dest_addr_))};
            bytes_sent < 0)
        {
            std::perror("sendto");
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
