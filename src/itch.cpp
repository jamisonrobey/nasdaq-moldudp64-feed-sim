#include "itch.h"

#include <chrono>
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <netdb.h>

using namespace Itch;

std::optional<std::span<const std::byte>> Itch::seek_next_message(std::span<const std::byte> file, std::size_t file_pos)
{
    if (file_pos + len_prefix_size > file.size())
    {
        return std::nullopt;
    }

    std::uint16_t len_prefix{};
    std::memcpy(&len_prefix, &file[file_pos], len_prefix_size);
    len_prefix = ntohs(len_prefix);

    const std::size_t total_msg_len = len_prefix_size + len_prefix;

    [[unlikely]]
    if (file_pos + total_msg_len > file.size())
    {
        throw std::runtime_error("ITCH message exceeds file size");
    }

    return std::span{file.subspan(file_pos, total_msg_len)};
}

std::chrono::nanoseconds Itch::extract_timestamp(std::span<const std::byte> bytes)
{
    if (bytes.size() < timestamp_size)
    {

        throw std::invalid_argument("Too small to contain a valid timestamp");
    }

    std::uint64_t timestamp = 0;
    for (std::size_t i = 0; i < timestamp_size; ++i)
    {
        timestamp = (timestamp << 8) | std::to_integer<std::uint64_t>(bytes[i]);
    }

    const auto timestamp_duration = std::chrono::nanoseconds{timestamp};
    if (timestamp_duration >= max_timestamp)
    {
        throw std::out_of_range("Timestamp exceeds 24 hours");
    }

    return timestamp_duration;
}
