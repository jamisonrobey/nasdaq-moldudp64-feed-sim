#include "timestamp.h"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace itch
{
    // bounds unchecked so caller must make sure this is safe
    std::chrono::nanoseconds extract_timestamp(std::span<const char> bytes)
    {
        // https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
        // all TotalView ITCH messages have timestamp at offset of 5
        static constexpr auto timestamp_offset{5UZ};
        static constexpr auto timestamp_length{6UZ};

        assert(bytes.size() >= timestamp_offset + timestamp_length && "extract_timestamp: not enough bytes");

        // copy into 64 bit, byteswap then shift 16 to align MSB
        std::uint64_t timestamp{0};
        std::memcpy(&timestamp, bytes.data() + timestamp_offset, timestamp_length);
        timestamp = std::byteswap(timestamp) >> 16;

        return std::chrono::nanoseconds{timestamp};
    }
}
