#include "gtest/gtest.h"
#include <chrono>
#include "itch.h"

namespace
{

constexpr std::array<std::uint8_t, Itch::timestamp_size> to_big_endian(std::uint64_t value)
{
    std::array<std::uint8_t, Itch::timestamp_size> bytes{};
    for (std::size_t i = 0; i < Itch::timestamp_size; ++i)
    {
        bytes[Itch::timestamp_size - 1 - i] = static_cast<std::uint8_t>(value & 0xFF);
        value >>= 8;
    }
    return bytes;
}

}

TEST(ItchTest, ExtractTimestamp_ZeroValue)
{
    constexpr auto bytes{to_big_endian(0)};
    const auto timestamp{Itch::extract_timestamp(bytes.data())};
    EXPECT_EQ(timestamp.count(), 0);
}

TEST(ItchTest, ExtractTimestamp_BigEndianParsing)
{
    constexpr auto bytes{to_big_endian(1000)};
    const auto timestamp{Itch::extract_timestamp(bytes.data())};
    EXPECT_EQ(timestamp.count(), 1000);
}

TEST(ItchTest, ExtractTimestamp_MaxValid)
{
    using namespace std::chrono;
    constexpr std::uint64_t max_valid{duration_cast<nanoseconds>(Itch::max_timestamp).count() - 1};
    constexpr auto bytes{to_big_endian(max_valid)};
    const auto timestamp{Itch::extract_timestamp(bytes.data())};
    EXPECT_EQ(timestamp.count(), max_valid);
}

TEST(ItchTest, ExtractTimestamp_ThrowsOnExceed24Hours)
{

    using namespace std::chrono;
    constexpr std::uint64_t over_limit{duration_cast<nanoseconds>(Itch::max_timestamp).count()};
    constexpr auto bytes{to_big_endian(over_limit)};
    EXPECT_THROW(Itch::extract_timestamp(bytes.data()), std::out_of_range);
}