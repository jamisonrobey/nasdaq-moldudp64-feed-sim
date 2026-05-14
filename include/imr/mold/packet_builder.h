#pragma once

#include "imr/mold/types.h"

#include <string_view>
#include <span>
#include <vector>

namespace imr::mold
{
    class PacketBuilder
    {
      public:
        struct Config
        {
            std::string_view session;
            std::size_t MTU{1472};
            std::uint64_t start_sequence{1};
        };
        PacketBuilder(const Config& cfg);

        [[nodiscard]]
        bool try_add(std::span<const char> message) noexcept;

        std::span<const char> finalize() noexcept;

        // reset msg_count and optional new sequence number
        void reset(SequenceNumber sequence_number = 1) noexcept;

        void set_message_count(MessageCount msg_count) noexcept;
        void set_sequence_number(SequenceNumber sequence_number) noexcept;

        [[nodiscard]]
        mold::MessageCount message_count() const noexcept;

      private:
        std::size_t MTU_;
        std::vector<char> buffer_;
        std::size_t write_pos_{0};
        mold::MessageCount msg_count_;
    };
}
