#include "itch.h"

#include <chrono>
#include <stdexcept>

std::chrono::nanoseconds Itch::extract_timestamp(const std::uint8_t* bytes)
{
    std::uint64_t timestamp{0};

    // read 6-byte BE and convert to uint64_t LE
    for (std::size_t i = 0; i < Itch::timestamp_size; ++i)
    {
        timestamp = (timestamp << 8) | bytes[i];
    }

    const auto timestamp_duration{std::chrono::nanoseconds{timestamp}};
    if (timestamp_duration >= Itch::max_timestamp)
    {
        throw std::out_of_range("Timestamp exceeds 24 hours");
    }

    return timestamp_duration;
}