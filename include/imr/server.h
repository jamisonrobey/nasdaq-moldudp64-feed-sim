#pragma once

#include "imr/util/memory_mapped_file.h"
#include "imr/mold/retransmission_buffer.h"
// #include "imr/mold/retransmission/feed_pool.h"
#include "imr/mold/downstream/feed.h"
#include <thread>

namespace imr
{
    class Server
    {
      public:
        struct Config
        {
            util::MemoryMappedFile::Config itch_file_cfg;
            // prefer pow 2
            mold::PacketBuilder::Config packet_builder_cfg{};
            mold::downstream::Feed::Config downstream_feed_cfg;
            //           mold::retransmission::Feed::Config retransmission_feed_cfg;
            std::size_t retransmission_buffer_size{1 << 22};
            std::size_t num_retransmission_feeds{std::thread::hardware_concurrency()};
        };

        explicit Server(const Config& cfg)
            : mapped_itch_file_(cfg.itch_file_cfg),
              retransmission_buffer_(cfg.retransmission_buffer_size),
              //            retransmission_feeds_(cfg.retransmission_feed_cfg, cfg.packet_builder_cfg, mapped_itch_file_.as_span(), retransmission_buffer_, cfg.num_retransmission_feeds),
              downstream_feed_(cfg.downstream_feed_cfg, cfg.packet_builder_cfg, mapped_itch_file_.as_span(), retransmission_buffer_)
        {
        }

        void start()
        {
            std::jthread downstream_thread([&]() {
                // block till EOF
                downstream_feed_.start();
                // signal retransmission to stop
                //            retransmission_feeds_.stop();
            });
        }

        void stop()
        {
            downstream_feed_.stop();
            //        retransmission_feeds_.stop();
        }

      private:
        imr::util::MemoryMappedFile mapped_itch_file_;
        imr::mold::RetransmissionBuffer retransmission_buffer_;
        //     imr::mold::retransmission::FeedPool retransmission_feeds_;
        imr::mold::downstream::Feed downstream_feed_;
    };
}
