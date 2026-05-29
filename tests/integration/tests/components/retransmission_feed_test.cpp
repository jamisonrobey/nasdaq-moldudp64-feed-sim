#include <gtest/gtest.h>
#include <sys/eventfd.h>

#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/retransmission/feed.h"

using namespace imr::mold;

class RetransmissionFeedTest : public ::testing::Test
{
  protected:
    RetransmissionBuffer retransmission_buffer{1};
    PacketBuilder::Config packet_builder_cfg{
        .session = "SESSION001",
    };

    retransmission::Feed make_feed(const retransmission::Feed::Config& cfg)
    {
        return retransmission::Feed(cfg, packet_builder_cfg, retransmission_buffer, eventfd(0, EFD_CLOEXEC));
    }
};

TEST_F(RetransmissionFeedTest, Ctor_ValidConfig_NoThrow)
{
    EXPECT_NO_THROW(make_feed({
        .address = "127.0.0.1",
        .port = 3500,
    }));
}

TEST_F(RetransmissionFeedTest, Ctor_InvalidAddress_ThrowsInvalidArgument)
{
    EXPECT_THROW(make_feed({.address = ""}), std::invalid_argument);
    EXPECT_THROW(make_feed({.address = "badip"}), std::invalid_argument);
}

TEST_F(RetransmissionFeedTest, Ctor_InvalidShutdownFd_ThrowsInvalidArugment)
{
    EXPECT_THROW(retransmission::Feed({.address = "127.0.0.1"}, packet_builder_cfg, retransmission_buffer, 0), std::invalid_argument);
    EXPECT_THROW(retransmission::Feed({.address = "127.0.0.1"}, packet_builder_cfg, retransmission_buffer, -1), std::invalid_argument);
}
