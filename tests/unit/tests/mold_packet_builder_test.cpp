#include <gtest/gtest.h>
#include "imr/mold/packet_builder.h"
#include "imr/mold/types.h"
#include <array>
#include <bit>
#include <cstring>

using namespace imr;

namespace
{
    constexpr mold::PacketBuilder::Config example_cfg{
        .session = "SESSION001",
        .MTU = 100,
    };

    constexpr std::array<char, mold::PacketBuilder::min_message_size_totalview_itch> min_msg{};

    template <typename T>
    T read_be(const void* base, std::size_t offset)
    {
        T value{};
        std::memcpy(&value, static_cast<const char*>(base) + offset, sizeof(T));
        return std::byteswap(value);
    }
}

TEST(MoldPacketBuilder, CopiesSession_ToStartOfHeader)
{
    mold::PacketBuilder packet_builder(example_cfg);

    auto packet{packet_builder.finalize()};

    EXPECT_EQ(
        std::string_view(static_cast<const char*>(packet.front().iov_base), example_cfg.session.size()),
        example_cfg.session);
}

TEST(MoldPacketBuilder, Session_ReturnsConfigSession)
{
    mold::PacketBuilder packet_builder(example_cfg);

    EXPECT_EQ(packet_builder.session(), example_cfg.session);
}

TEST(MoldPacketBuilder, TryAdd_ReturnsTrueAndIncrementsCount_ForValidMessage)
{
    mold::PacketBuilder packet_builder(example_cfg);

    EXPECT_TRUE(packet_builder.try_add(min_msg));
    EXPECT_EQ(packet_builder.message_count(), 1);
}

TEST(MoldPacketBuilder, TryAdd_ReturnsFalse_WhenMTUExceeded)
{
    mold::PacketBuilder packet_builder(example_cfg);
    while (packet_builder.try_add(min_msg))
    {
    }

    EXPECT_FALSE(packet_builder.try_add(min_msg));
}

TEST(MoldPacketBuilder, TryAdd_ReturnsFalse_ForEmptyMessage)
{
    mold::PacketBuilder packet_builder(example_cfg);

    constexpr std::array<char, 0> empty{};
    EXPECT_FALSE(packet_builder.try_add(empty));
}

TEST(MoldPacketBuilder, TryAdd_MessageCountAccumulates)
{
    mold::PacketBuilder packet_builder(example_cfg);
    ASSERT_TRUE(packet_builder.try_add(min_msg));
    ASSERT_TRUE(packet_builder.try_add(min_msg));

    EXPECT_EQ(packet_builder.message_count(), 2);
}

TEST(MoldPacketBuilder, Finalize_WritesMessageCountToHeader)
{
    mold::PacketBuilder packet_builder(example_cfg);
    ASSERT_TRUE(packet_builder.try_add(min_msg));
    ASSERT_TRUE(packet_builder.try_add(min_msg));

    auto packet{packet_builder.finalize()};

    const auto count{read_be<mold::types::header::MessageCount>(
        packet.front().iov_base,
        mold::types::header::message_count_offset)};
    EXPECT_EQ(count, 2);
}

TEST(MoldPacketBuilder, Finalize_ReturnsHeaderSlotPlusOneIovecPerMessage)
{
    mold::PacketBuilder packet_builder(example_cfg);
    ASSERT_TRUE(packet_builder.try_add(min_msg));
    ASSERT_TRUE(packet_builder.try_add(min_msg));

    auto packet{packet_builder.finalize()};

    EXPECT_EQ(packet.size(), 3); // header + 2 messages
    EXPECT_EQ(packet[0].iov_len, mold::types::header::length);
    EXPECT_EQ(packet[1].iov_len, min_msg.size());
    EXPECT_EQ(packet[2].iov_len, min_msg.size());
}

TEST(MoldPacketBuilder, Finalize_MessageIovecs_PointToOriginalData)
{
    mold::PacketBuilder packet_builder(example_cfg);
    std::array<char, mold::PacketBuilder::min_message_size_totalview_itch> msg{'A', 'B', 'C'};
    ASSERT_TRUE(packet_builder.try_add(msg));

    auto packet{packet_builder.finalize()};

    // iov_base must point into the original buffer
    EXPECT_EQ(packet[1].iov_base, msg.data());
}

TEST(MoldPacketBuilder, Reset_ClearsMessageCount)
{
    mold::PacketBuilder packet_builder(example_cfg);
    ASSERT_TRUE(packet_builder.try_add(min_msg));
    ASSERT_TRUE(packet_builder.try_add(min_msg));

    packet_builder.reset(1);

    EXPECT_EQ(packet_builder.message_count(), 0);
}

TEST(MoldPacketBuilder, Reset_AllowsRefill)
{
    mold::PacketBuilder packet_builder(example_cfg);
    while (packet_builder.try_add(min_msg))
    {
    }

    packet_builder.reset(1);

    EXPECT_TRUE(packet_builder.try_add(min_msg));
}

TEST(MoldPacketBuilder, Reset_WritesSequenceNumberToHeader)
{
    mold::PacketBuilder packet_builder(example_cfg);

    packet_builder.reset(42);

    auto packet{packet_builder.finalize()};
    const auto seq{read_be<mold::types::header::SequenceNumber>(
        packet.front().iov_base,
        mold::types::header::sequence_number_offset)};
    EXPECT_EQ(seq, 42);
}
