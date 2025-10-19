#ifndef MOLD_UDP_64_H
#define MOLD_UDP_64_H

#include <cstddef>
#include <string_view>
#include <cstring>
#include <chrono>

namespace MoldUDP64
{

inline constexpr std::size_t session_id_size{10};

inline constexpr std::size_t mtu_size{1200}; // this is left with a little headroom for example VPN
inline constexpr std::size_t udp_header_size{28};
inline constexpr std::size_t max_payload_size{mtu_size - udp_header_size};

inline constexpr std::uint16_t end_of_session_flag{0xFFFF};
inline constexpr std::chrono::seconds end_of_session_duration{30};

using Session = std::array<char, session_id_size>;

struct [[gnu::packed]] DownstreamHeader
{
    Session session{};
    std::uint64_t sequence_num{};
    std::uint16_t msg_count{};

    DownstreamHeader() = default;
    explicit DownstreamHeader(std::string_view session_);
    explicit DownstreamHeader(const Session& session_);
};

static_assert(std::is_trivially_copyable_v<DownstreamHeader>);

using RetransmissionRequest = DownstreamHeader;

using Packet = std::array<std::byte, max_payload_size>;
}
#endif
