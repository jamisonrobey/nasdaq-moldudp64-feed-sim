#include <gtest/gtest.h>

#include "mold/packet_builder.h"
#include "mold/types.h"

#include <print>

namespace
{
    constexpr std::string_view example_session{"SESSION001"};
    std::array one_byte_msg{'0'};
};

TEST(MoldPacketBuilder, ConstructorCopiesSesssionToStartOfPacket)
{
    mold::PacketBuilder packet_builder(example_session);

    // immediately call finalize cause we only need to inspect the session which is copied in ctor
    std::span packet{packet_builder.finalize()};

    EXPECT_EQ(std::string_view(packet.data(), example_session.size()), example_session);
}

TEST(MoldPacketBuilder, AddMessage)
{
    mold::PacketBuilder packet_builder(example_session);

    EXPECT_TRUE(packet_builder.try_add(one_byte_msg));
    EXPECT_EQ(packet_builder.message_count(), 1);
}

TEST(MoldPacketBuilder, AddMessageWhenCantFit)
{
    constexpr auto mtu{50UZ};
    mold::PacketBuilder packet_builder(example_session, mtu + mold::header_length);

    for (auto i{0UZ}; i < mtu; ++i)
    {
        EXPECT_TRUE(packet_builder.try_add(one_byte_msg));
    }

    EXPECT_FALSE(packet_builder.try_add(one_byte_msg));
}

TEST(MoldPacketBuilder, TryAddEmptyMessageReturnsFalse)
{
    mold::PacketBuilder packet_builder(example_session);
    static constexpr std::array<char, 0> empty_msg{};

    EXPECT_FALSE(packet_builder.try_add(empty_msg));
}
