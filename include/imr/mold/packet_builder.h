#pragma once

#include "imr/mold/types.h"
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <sys/uio.h>
#include <vector>

namespace imr::mold
{
    // builds MoldUDP64 packets
    // How to use:
    //  1. Call reset to write sequence number to the header
    //  2. add messages with try_add() until either added all or try_add() returns false indicating a full packet (according to the MTU given in ctor)
    //  3. Call finalize to write the message count to the header and get a span of iovecs that contains the full packet
    class PacketBuilder
    {
      public:
        // for total view itch
        // https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/nqtvitchspecification.pdf
        // smallest message 12-bytes + 2-byte length prefix
        static constexpr std::uint16_t min_message_size{14};

        struct Config
        {
            // exactly 10 characters or throws std::invalid_argument
            std::string_view session;
            // must be >= types::header::length (20 bytes) or throws std::invalid_argument
            std::size_t MTU{1472};
            // minimum size of message to be passed to try_add()
            std::size_t min_message_size{PacketBuilder::min_message_size};
        };

        explicit PacketBuilder(const Config& cfg);

        // you can add whatever but this is intended to be messages that should be apart of the mold
        // message block.
        [[nodiscard]]
        bool try_add(std::span<const char> message) noexcept;
        // writes message_count to header, returns header + msg block as iovec span for msghdr
        [[nodiscard]]
        std::span<iovec> finalize() noexcept;
        // writes sequence number to header and clears message block
        void reset(types::header::SequenceNumber seq = 1) noexcept;

        [[nodiscard]]
        types::header::MessageCount message_count() const noexcept;
        [[nodiscard]]
        std::string_view session() const noexcept;

      private:
        std::size_t MTU_;
        std::size_t bytes_remaining_;
        std::size_t min_message_size_;

        std::array<char, types::header::length> header_buffer_{};
        // [0] = header (*iov_base = header_buffer_), [1..N] = messages
        // N = 1 (header) + (MTU_ / min_message_size_)
        std::vector<iovec> iovecs_;
    };
}
