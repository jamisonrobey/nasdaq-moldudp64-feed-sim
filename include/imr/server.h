#pragma once

#include "imr/util/memory_mapped_file.h"
#include "imr/mold/downstream/feed.h"
#include "imr/mold/retransmission_buffer.h"

namespace imr
{
    class Server
    {
      public:
        struct Config
        {
            util::MemoryMappedFile::Config itch_file_cfg;
            // prefer pow 2
            std::size_t retransmission_buffer_size{1 << 22};
            mold::PacketBuilder::Config packet_builder_cfg;
            mold::downstream::Feed::Config downstream_feed_cfg;
        };

        explicit Server(const Config& cfg)
            : mapped_itch_file_(cfg.itch_file_cfg),
              retransmission_buffer_(cfg.retransmission_buffer_size),
              downstream_feed_(cfg.downstream_feed_cfg, cfg.packet_builder_cfg, mapped_itch_file_.as_span(), retransmission_buffer_) {}

        void start()
        {
            downstream_feed_.start();
        }

      private:
        imr::util::MemoryMappedFile mapped_itch_file_;
        imr::mold::RetransmissionBuffer retransmission_buffer_;
        imr::mold::downstream::Feed downstream_feed_;
    };
}
