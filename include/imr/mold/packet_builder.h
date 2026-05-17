#pragma once

#include "imr/mold/types.h"

#include <string_view>
#include <span>
#include <sys/uio.h>
#include <vector>

namespace imr::mold
{
    class PacketBuilder
    {
      public:
        // comes from https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
        // which shows smallest message at 10-bytes + the 2-byte length prefix that proceeds each message in the file = 12
        static constexpr auto min_message_size_totalview_itch{12};

        struct Config
        {
            std::string_view session;
            std::size_t MTU{1472};
            // used to size iovec buffer (smaller = more memory). change only if the itch you use has
            // different minimum mesasge size (including any prefix in the file)
            std::size_t min_message_size{min_message_size_totalview_itch};
        };

        explicit PacketBuilder(const Config& cfg);

        [[nodiscard]]
        bool try_add(std::span<const char> message) noexcept;

        // write message_count_ to header and return full iovec for packet
        [[nodiscard]]
        std::span<iovec> finalize() noexcept;

        // reset and optionally advance sequence
        void reset(types::header::SequenceNumber sequence_number = 1) noexcept;

        // NOT written to header yet
        [[nodiscard]]
        types::header::MessageCount message_count() const noexcept;
        // written once in ctor
        [[nodiscard]] std::string_view session() const noexcept;

      private:
        [[nodiscard]]
        std::span<iovec> message_iovecs() noexcept;

        [[nodiscard]]
        std::span<iovec> sendmsg_iovecs() noexcept;

        std::size_t MTU_;
        std::array<char, types::header::length> header_{};
        std::vector<iovec> iovecs_; // [0]=header, [1..size()-1]=messages
        types::header::MessageCount message_count_{0};
        std::size_t bytes_used_{types::header::length};

        std::size_t min_message_size_;
    };

}
