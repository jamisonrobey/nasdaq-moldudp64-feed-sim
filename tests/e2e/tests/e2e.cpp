#include <chrono>

#include <gtest/gtest.h>
#include <test_file_fixture.h>

namespace
{
    imr::Server::Config config{
        .packet_builder_cfg =
            {
                .session = "SESSION001",
            },
        .downstream_feed_config =
            {
                .mcast_group = "239.0.0.1",
                .heartbeat_period = std::chrono::milliseconds(50),
                .end_of_session_duration = std::chrono::milliseconds(500),
            },
        .retransmission_feed_config =
            {
                .address = "127.0.0.1",
            },
    };

}

class E2ETest : public test_common::ItchFileFixture<1024>
{
};

TEST_F(E2ETest, Test)
{
    EXPECT_TRUE(true);
}
