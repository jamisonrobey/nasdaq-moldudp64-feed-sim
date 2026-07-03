#include "imr/mold/retransmission/feed_pool.h"
#include "imr/util/file_descriptor.h"
#include "imr/util/log.h"

#include <source_location>
#include <unistd.h>

namespace imr::mold::retransmission
{
    FeedPool::FeedPool(std::size_t num_feeds,
                       const Feed::Config& feed_cfg,
                       const PacketBuilder::Config& packet_builder_cfg,
                       std::span<const char> file,
                       const RetransmissionBuffer& retransmission_buffer)
        : feed_cfg_{&feed_cfg},
          packet_builder_cfg_{&packet_builder_cfg}
    {
        feeds_.reserve(num_feeds);
        for (auto i{0UZ}; i < num_feeds; ++i)
        {
            feeds_.emplace_back([this, &retransmission_buffer, file] {
                Feed feed(*feed_cfg_, *packet_builder_cfg_, file, retransmission_buffer, shutdown_fd_.get());
                feed.start();
            });

            util::log::info("Started retransmission thread {} of {}", i + 1, num_feeds);
        }

        util::log::debug();
    }

    void FeedPool::stop() const
    {
        constexpr std::uint64_t val{1};
        if (write(shutdown_fd_.get(), &val, sizeof(val)) < 0)
        {
            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }

        util::log::debug();
    }
};
