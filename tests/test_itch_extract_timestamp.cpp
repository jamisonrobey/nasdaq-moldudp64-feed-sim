#include "gtest/gtest.h"
#include <chrono>
#include "itch.h"

namespace
{
// u64 into itch timestamp (6-byte big-endian)
constexpr std::array<std::byte, Itch::timestamp_size> encode_big_endian_timestamp(std::uint64_t value)
{
    std::array<std::byte, Itch::timestamp_size> bytes{};
    for (std::size_t i = 0; i < Itch::timestamp_size; ++i)
    {
        bytes[Itch::timestamp_size - 1 - i] = static_cast<std::byte>(value & 0xFF);
        value >>= 8;
    }
    return bytes;
}
}

TEST(ItchTest, ExtractTimestamp_ZeroValue)
{
    constexpr auto bytes{encode_big_endian_timestamp(0)};
    const auto timestamp{Itch::extract_timestamp(bytes)};
    EXPECT_EQ(timestamp.count(), 0);
}

TEST(ItchTest, ExtractTimestamp_BigEndianParsing)
{
    constexpr auto bytes{encode_big_endian_timestamp(1000)};
    const auto timestamp{Itch::extract_timestamp(bytes)};
    EXPECT_EQ(timestamp.count(), 1000);
}

TEST(ItchTest, ExtractTimestamp_MaxValid)
{
    using namespace std::chrono;
    constexpr std::uint64_t max_valid{duration_cast<nanoseconds>(Itch::max_timestamp).count() - 1};
    constexpr auto bytes{encode_big_endian_timestamp(max_valid)};
    const auto timestamp{Itch::extract_timestamp(bytes)};
    EXPECT_EQ(timestamp.count(), max_valid);
}

TEST(ItchTest, ExtractTimestamp_ThrowsOnExceed24Hours)
{

    using namespace std::chrono;
    constexpr std::uint64_t over_limit{duration_cast<nanoseconds>(Itch::max_timestamp).count()};
    constexpr auto bytes{encode_big_endian_timestamp(over_limit)};
    EXPECT_THROW(Itch::extract_timestamp(bytes), std::out_of_range);
}
