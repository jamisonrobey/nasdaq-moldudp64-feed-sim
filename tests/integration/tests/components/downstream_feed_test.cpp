#include <gtest/gtest.h>
#include "imr/mold/downstream/feed.h"
#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission_buffer.h"

using namespace imr::mold;

namespace
{
    constexpr std::array file{'a', 'b', 'c'};
}

class DownstreamFeedTest : public ::testing::Test
{
  protected:
    RetransmissionBuffer retransmission_buffer{1};
    PacketBuilder::Config packet_builder_cfg{
        .session = "SESSION001",
    };

    downstream::Feed make_feed(const downstream::Feed::Config& cfg)
    {
        return downstream::Feed(cfg, packet_builder_cfg, file, retransmission_buffer);
    }
};

TEST_F(DownstreamFeedTest, Ctor_ValidConfig_NoThrow)
{
    EXPECT_NO_THROW(make_feed({
        .mcast_group = "239.0.0.1",
        .port = 3400,
    }));
}

TEST_F(DownstreamFeedTest, Ctor_InvalidMcastGroup_ThrowsInvalidArgument)
{
    EXPECT_THROW(make_feed({.mcast_group = ""}), std::invalid_argument);
    EXPECT_THROW(make_feed({.mcast_group = "badip"}), std::invalid_argument);
}
