#ifndef ITCH_H
#define ITCH_H

#include <cstddef>
#include <chrono>

// https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
namespace Itch
{
// replay file from emi.nasdaq prefixed with 16-bit len for each message
inline constexpr std::size_t len_prefix_size{2};

inline constexpr std::size_t max_msg_len{50};

inline constexpr std::size_t timestamp_size{6};

inline constexpr std::size_t timestamp_offset{
    len_prefix_size +
    5 /* message type + stock_locate + tracking number*/};

inline constexpr auto max_timestamp{std::chrono::hours{24}};

// reads 6-byte big-endian timestamp as u64 then returns as chrono::nanoseconds
std::chrono::nanoseconds extract_timestamp(const std::uint8_t* bytes);

}

#endif
