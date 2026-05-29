#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/util/memory_mapped_file.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/retransmission/feed_pool.h"
#include "imr/mold/downstream/feed.h"

#include <thread>
#include <memory>
#include <expected>

namespace imr
{
    class Server
    {
      public:
        // each config's declaration details their values in depth. alternatively, check examples directory
        struct Config
        {
            util::MemoryMappedFile::Config mapped_itch_file_cfg;
            mold::PacketBuilder::Config packet_builder_cfg;
            mold::downstream::Feed::Config downstream_feed_config;
            // pow2 buffer_size will use bitmasking for index lookup (better perf), non-pow2 falls back to division
            std::size_t retransmission_buffer_size{1 << 22U};
            mold::retransmission::Feed::Config retransmission_feed_config;
            //  hardware_concurrency() - 1 for downstream thread but this is just a default
            std::size_t num_retransmission_feeds{std::thread::hardware_concurrency() - 1};
        };

        explicit Server(const Config& cfg);

        // start downstream and run to file completion then stop feeds
        void start();

        // block until downstream finishes
        void wait_for_downstream();

        // stop early
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

    // use this if don't want to try/catch the ctor directly
    std::expected<std::unique_ptr<Server>, std::string> make_server(const Server::Config& cfg) noexcept;

}
