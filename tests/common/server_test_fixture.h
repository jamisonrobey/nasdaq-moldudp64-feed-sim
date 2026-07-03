#pragma once

#include <bit>
#include <gtest/gtest.h>

#include <imr/server.h>
#include <imr/util/file_descriptor.h>

#include <cstdint>
#include <memory>
#include <format>
#include <expected>
#include <bit>

#include <netinet/in.h>
#include <sys/socket.h>

namespace test_common
{
    template <typename FileFixture>
    class ServerTestFixture : public FileFixture
    {
      protected:
        static std::uint16_t find_free_udp_port()
        {
            imr::util::FileDescriptor probe{socket(AF_INET, SOCK_DGRAM, 0)};

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            // let kernel assign
            addr.sin_port = 0;

            EXPECT_EQ(bind(probe.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0)
                << "find_free_udp_port: bind failed: " << std::strerror(errno);

            socklen_t len{sizeof(addr)};
            EXPECT_EQ(getsockname(probe.get(), reinterpret_cast<sockaddr*>(&addr), &len), 0)
                << "find_free_udp_port: getsockname failed: " << std::strerror(errno);

            return std::byteswap(addr.sin_port);
        }

        static std::unique_ptr<imr::Server> make_test_server(imr::Server::Config& config)
        {
            config.mapped_itch_file_cfg.path = FileFixture::test_path();
            config.retransmission_feed_config.port = find_free_udp_port();

            std::expected result{imr::make_server(config)};

            EXPECT_TRUE(result.has_value()) << std::format("Error during server construction: {}", result.error());

            return std::move(*result);
        }
    };
}
