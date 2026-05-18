#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/util/memory_mapped_file.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/retransmission/feed_pool.h"
#include "imr/mold/downstream/feed.h"

namespace imr
{
    class Server
    {
      public:
        struct Config
        {
            std::filesystem::path file_path;
            std::size_t retransmission_buffer_size{1 << 22U};
            mold::PacketBuilder::Config packet_builder_cfg;
            mold::retransmission::Feed::Config retransmission_feed_config;
            mold::downstream::Feed::Config downstream_feed_config;
        };

        explicit Server(const Config& cfg);

        void start();

        void stop();

      private:
        util::MemoryMappedFile file_;
        mold::RetransmissionBuffer retransmission_buffer_;
        mold::retransmission::FeedPool retransmission_feeds_;
        mold::downstream::Feed downstream_feed_;
    };
}
