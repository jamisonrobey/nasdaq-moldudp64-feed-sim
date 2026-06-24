#include <gtest/gtest.h>

#include <memory>
#include <test_file_fixture.h>

#include <imr/server.h>

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
                .end_of_session_duration = std::chrono::seconds(0),
                .pacer_cfg =
                    {
                        .skip_before = std::chrono::nanoseconds(0),
                    },
            },
        .retransmission_feed_config =
            {
                .address = "127.0.0.1",
            },
    };

}

class ServerIntegrationTest : public test_common::ItchFileFixture<1024>
{
};

TEST_F(ServerIntegrationTest, Start_RunToEOF)
{
    const std::unique_ptr<imr::Server> server{make_test_server(config)};

    server->start();
    server->wait_for_downstream();
}

TEST_F(ServerIntegrationTest, Stop_WhileRunning)
{

    const std::unique_ptr<imr::Server> server{make_test_server(config)};

    server->start();
    server->stop();
}

TEST_F(ServerIntegrationTest, Stop_AfterEOF)
{
    const std::unique_ptr<imr::Server> server{make_test_server(config)};

    server->start();
    server->wait_for_downstream();
    server->stop();
}
