#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/util/memory_mapped_file.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/retransmission/feed_pool.h"
#include "imr/mold/downstream/feed.h"

#include <thread>
#include <memory>
#include <expected>
#include <algorithm>

/**
 *
 * @code{.cpp}
 * imr::Server::Config cfg{...};
 *
ndi  * // construct directly with try/catch
 *
 * try
 * {
 *   Server server(cfg);
 *   server.start();
 *   server.wait_for_downstream();
 *  }
 *   catch (const std::exception& ex)
 *  {
 *   std::println("{}", ex.what());
 *  }
 *
 *  // or use helper that catches and returns std::expected
 *
 *  const std::expected res{imr::make_server(cfg)};
 *  if (!res.has_value())
 *  {
 *    std::println(stderr, "{}", res.error());
 *  }
 *
 *  std::unique_ptr<imr::Server> server{*res};
 *
 *  server->start();
 *  server->wait_for_downstream();
 * @endcode
 *
 */
namespace imr
{

    /// A MoldUDP64 server that replays NASDAQ TotalView-ITCH 5.0 files.
    class Server
    {
      public:
        /// Aggregate for configuration of all server components.
        struct Config
        {
            util::MemoryMappedFile::Config mapped_itch_file_cfg;
            mold::PacketBuilder::Config packet_builder_cfg;
            mold::downstream::Feed::Config downstream_feed_config;
            /**
             Capacity of the retransmission ring buffer in messages.

             Power-of-two sizes use bitmasking for index lookup (faster); non-power-of-two sizes fall back to division.
             */
            std::size_t retransmission_buffer_size{1 << 22U};
            mold::retransmission::Feed::Config retransmission_feed_config;
            /**
             Number of retransmission feed worker threads.

             Defaults to either 1 or one less than the hardware thread count for multicore systems to leave one thread for the downstream feed
             */
            std::size_t num_retransmission_feeds{std::max(std::thread::hardware_concurrency() - 1, 1U)};
        };

        /**
         Constructs server ready to start

         @throws std::invalid_arugment if configuration passed is invalid
         @throws std::system_error if system calls to setup server fail
        */
        explicit Server(const Config& cfg);

        /**
         Starts downstream replay and retransmission feeds.

         Downstreams and retransmission feeds run in background threads until downstream reaches end of file and finishes sending
         downstream packets
         */
        void start();

        /// Blocks caller until downstream replay finishes.
        void wait_for_downstream();

        /// Stop all feeds early (before downstream reaches end of file).
        void stop();

        ~Server();

        Server(const Server&) = delete;
        Server(Server&&) = delete;

        Server& operator=(const Server&) = delete;
        Server& operator=(Server&&) = delete;

      private:
        util::MemoryMappedFile mapped_itch_file_;
        mold::RetransmissionBuffer retransmission_buffer_;
        mold::downstream::Feed downstream_feed_;
        std::jthread downstream_thread_;
        mold::retransmission::FeedPool retransmission_feeds_;

        void join_downstream();
    };

    /**
     Allows you to construct server without try/catch block to handle configuration errors.

     At the moment just returns a string describing the error, so you can print and see what went wrong.
    */
    std::expected<std::unique_ptr<Server>, std::string> make_server(const Server::Config& cfg) noexcept;

}
