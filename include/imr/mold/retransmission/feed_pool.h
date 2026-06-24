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
        FeedPool(std::size_t num_feeds,
                 const Feed::Config& feed_cfg,
                 const PacketBuilder::Config& packet_builder_cfg,
                 std::span<const char> file,
                 const RetransmissionBuffer& retransmission_buffer);

        void stop() const;

      private:
        util::FileDescriptor shutdown_fd_{eventfd(0, EFD_CLOEXEC)};
        std::vector<std::jthread> feeds_;
        Feed::Config feed_cfg_;
        PacketBuilder::Config packet_builder_cfg_;
    };
}
