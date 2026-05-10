#pragma once

#include "mold/types.h"

#include <string_view>
#include <span>
#include <vector>

namespace mold
{
    // 1500 byte ethernet MTU minus IP (20) and UDP (8) headers
    static constexpr auto default_MTU{1472UZ};

    class PacketBuilder
    {
      public:
        PacketBuilder(std::string_view session, std::size_t MTU = default_MTU);

        [[nodiscard]]
        bool try_add(std::span<const char> message);

        std::span<const char> finalize();

        void reset(mold::SequenceNumber sequence_number);

        [[nodiscard]]
        mold::MessageCount message_count() const noexcept;

      private:
        std::size_t MTU_;
        std::vector<char> buffer_;
        std::size_t write_pos_{0};
        mold::MessageCount msg_count_;
    };
}
