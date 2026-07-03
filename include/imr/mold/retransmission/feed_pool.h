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
        /**
         Constructs and starts `num_feeds` retransmission feed threads

         @throws std::invalid_argument if the shutdown eventfd fails to be created.
        */
        FeedPool(std::size_t num_feeds,
                 const Feed::Config& feed_cfg,
                 const PacketBuilder::Config& packet_builder_cfg,
                 std::span<const char> file,
                 const RetransmissionBuffer& retransmission_buffer);

        void stop() const;

      private:
        util::FileDescriptor shutdown_fd_{[] { return eventfd(0, EFD_CLOEXEC); }};

        std::vector<std::jthread> feeds_;

        // we have to store these as members otherwise have to copy them N times in constructor
        // (if we pass reference from constructor then it can go out of scope before threads have finished using them)
        const Feed::Config* feed_cfg_;
        const PacketBuilder::Config* packet_builder_cfg_;
    };
}
