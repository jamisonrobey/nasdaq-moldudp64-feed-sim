#pragma once

#include "../mold/types.h"

#include <vector>
#include <atomic>
#include <cstddef>
#include <optional>

namespace mold
{

    // circular array, sequence number as index
    // single writer (downstream), N readers (retransmission), acquire/release on write_seq_ guards buffer visibility across threads
    class RetransmissionBuffer
    {
      public:
        struct MessageRecord
        {
            SequenceNumber sequence_number;
            std::size_t file_position;
        };

        // throws if buffer_size is not a power of 2, vector size not known at compile time so wont optimize
        // with bitmask for index lookup, enforcing pow of 2 lets us do it manually and can still have buffer size as a runtime argument
        explicit RetransmissionBuffer(std::size_t buffer_size);

        // only ever called with inplace construction (downstream)
        void push(MessageRecord&& message_record) noexcept;

        [[nodiscard]]
        std::optional<std::size_t> file_position_for(SequenceNumber sequence_number) const noexcept;

        [[nodiscard]]
        std::size_t size() const noexcept;

      private:
        std::vector<MessageRecord> buffer_;
        std::size_t mask_;
        alignas(64) std::atomic<SequenceNumber> write_seq_{0};

        [[nodiscard]]
        std::size_t index_for(SequenceNumber sequence_number) const noexcept;
    };
}
