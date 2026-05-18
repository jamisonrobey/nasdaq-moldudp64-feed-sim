#pragma once

#include "imr/mold/packet_builder.h"
#include "imr/mold/retransmission/feed.h"
#include "imr/mold/retransmission_buffer.h"

#include <sys/eventfd.h>
#include <vector>
#include <thread>

namespace imr::mold::retransmission
{
    class FeedPool
    {
      public:
        FeedPool(std::size_t num_threads,
                 const Feed::Config& feed_cfg,
                 const PacketBuilder::Config& packet_builder_cfg,
                 const RetransmissionBuffer& retransmission_buffer);

        void stop() const;

      private:
        std::vector<std::jthread> feeds_;
        Feed::Config feed_cfg_;
        PacketBuilder::Config packet_builder_cfg_;

        int shutdown_fd_{eventfd(0, EFD_CLOEXEC)};
    };
}
