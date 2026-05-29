#pragma once

#include "imr/mold/types.h"
#include "imr/util/debug_field.h"
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <sys/uio.h>
#include <vector>

namespace imr::mold
{
    class PacketBuilder
    {
      public:
        // https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
        // smallest message 10-bytes + 2-byte length prefix in the file = 12
        static constexpr std::uint16_t min_message_size_totalview_itch{12};

        struct Config
        {
            // exactly 10 characters or throws std::invalid_argument
            std::string_view session;
            // must be >= types::header::length (20 bytes) or throws std::runtime_error
            std::size_t MTU{1472};
            // minimum size of messages passed to try_add(), including any per-message file prefix.
            // used to reserve iovec capacity, only change if not using totalview itch 5.0
            std::size_t min_message_size{min_message_size_totalview_itch};
        };

        explicit PacketBuilder(const Config& cfg);

        // returns false if message is empty or would exceed MTU
        [[nodiscard]]
        bool try_add(std::span<const char> message) noexcept;
        // writes message_count to header, returns header + msg block as iovec span for msghdr
        [[nodiscard]]
        std::span<iovec> finalize() noexcept;
        // writes sequence number to header and clears messages
        void reset(types::header::SequenceNumber seq = 1) noexcept;

        [[nodiscard]]
        types::header::MessageCount message_count() const noexcept;
        [[nodiscard]]
        std::string_view session() const noexcept;

      private:
        std::size_t MTU_;
        std::size_t bytes_remaining_;
        [[no_unique_address]] util::DebugField<std::size_t> min_message_size_;

        std::array<char, types::header::length> header_buffer_{};
        // [0] = header (*iov_base = header_buffer_), [1..] = messages
        std::vector<iovec> iovecs_;
    };
}
