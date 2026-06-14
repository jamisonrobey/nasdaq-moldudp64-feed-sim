#include <gtest/gtest.h>

#include <test_file_fixture.h>

#include <imr/server.h>

namespace
{
    class ServerTest : public test_common::TestFileFixture<1024>
    {
      protected:
        static std::unique_ptr<imr::Server> make_test_server(std::uint16_t downstream_port, std::uint16_t retransmission_port)
        {
            std::expected result{imr::make_server(imr::Server::Config{
                .mapped_itch_file_cfg = {
                    .path = test_path,
                },
                .packet_builder_cfg = {
                    .session = "SESSION001",
                },
                .downstream_feed_config = {
                    .mcast_group = "239.0.0.1",
                    .port = downstream_port,
                    .pacer_cfg = {
                        .skip_before = imr::mold::downstream::market_open,
                    },
                },
                .retransmission_feed_config = {
                    .address = "127.0.0.1",
                    .port = retransmission_port,
                },
            })};

            EXPECT_TRUE(result.has_value()) << std::format("Error during server construction: {}", result.error());

            return std::move(*result);
        }
    };
}

TEST_F(ServerTest, Start_RunToEOF)
{
    const std::unique_ptr<imr::Server> server{make_test_server(3400, 3500)};

    server->start();
    server->wait_for_downstream();
}

TEST_F(ServerTest, Stop_WhileRunning)
{

    const std::unique_ptr<imr::Server> server{make_test_server(4400, 4500)};

    server->start();
    server->stop();
}

TEST_F(ServerTest, Stop_AfterEOF)
{
    const std::unique_ptr<imr::Server> server{make_test_server(5400, 5500)};

    server->start();
    server->wait_for_downstream();
    server->stop();
}
