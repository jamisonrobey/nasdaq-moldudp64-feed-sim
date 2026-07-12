#include <array>
#include <gtest/gtest.h>

#include "itch/timestamp.h"

class ItchTimestampTest : public ::testing::Test
{
  protected:
    // minimum "packet" for extract timestamp is array of bytes with length 11 (timestamp offset(5) + length(6))
    using MinimumPacket = std::array<char, 11>;
    MinimumPacket packet{};
};

TEST_F(ItchTimestampTest, ReturnsCorrectValue_ByteswappingFromNetworkOrder)
{
    packet[5] = 0x00;
    packet[6] = 0x01;
    packet[7] = 0x02;
    packet[8] = 0x03;
    packet[9] = 0x04;
    packet[10] = 0x05;

    const auto timestamp{itch::extract_timestamp(std::span(packet))};

    EXPECT_EQ(timestamp.count(), 0x000102030405);
}

TEST_F(ItchTimestampTest, ReturnsZero_ForZeroedPacket)
{
    const auto timestamp{itch::extract_timestamp(std::span(packet))};

    EXPECT_EQ(timestamp.count(), 0);
}
