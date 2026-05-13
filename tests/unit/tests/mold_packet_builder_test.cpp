#include <gtest/gtest.h>

#include "imr/mold/packet_builder.h"

using namespace imr;

namespace
{
    constexpr mold::PacketBuilder::Config example_cfg{
        .session = "SESSION001",
        .MTU = 50,
    };
    std::array one_byte_msg{'0'};
};

TEST(MoldPacketBuilder, CopiesSession_ToStartOfPacket)
{
    mold::PacketBuilder packet_builder(example_cfg);
    std::span packet{packet_builder.finalize()};

    EXPECT_EQ(std::string_view(packet.data(), example_cfg.session.size()), example_cfg.session);
}

TEST(MoldPacketBuilder, ReturnsTrueAndIncrementsCount_ForValidMessage)
{
    mold::PacketBuilder packet_builder(example_cfg);

    EXPECT_TRUE(packet_builder.try_add(one_byte_msg));
    EXPECT_EQ(packet_builder.message_count(), 1);
}

TEST(MoldPacketBuilder, ReturnsFalse_WhenCapacityExceeded)
{
    mold::PacketBuilder packet_builder(example_cfg);
    for (auto i{0UZ}; i < example_cfg.MTU; ++i)
    {
        // explicitly ignore [[nodiscard]]
        std::ignore = packet_builder.try_add(one_byte_msg);
    }

    EXPECT_FALSE(packet_builder.try_add(one_byte_msg));
}

TEST(MoldPacketBuilder, ReturnsFalse_ForEmptyMessage)
{
    mold::PacketBuilder packet_builder(example_cfg);
    static constexpr std::array<char, 0> empty_msg{};

    EXPECT_FALSE(packet_builder.try_add(empty_msg));
}
