#pragma once

#include "imr/mold/types.h"

#include <vector>
#include <atomic>
#include <cstddef>
#include <optional>

namespace imr::mold
{
    // circular array, sequence number as index
    // single writer (downstream), N readers (retransmission), acquire/release on write_seq_ guards buffer visibility across threads
    class RetransmissionBuffer
    {
      public:
        explicit RetransmissionBuffer(std::size_t buffer_size);

        struct MessageRecord
        {
            types::header::SequenceNumber sequence_number;
            std::size_t file_position;
        };

        void push(MessageRecord&& message_record) noexcept;

        [[nodiscard]]
        std::optional<types::header::SequenceNumber> file_position_for(types::header::SequenceNumber seq_num) const noexcept;

        [[nodiscard]]
        std::size_t size() const noexcept;

      private:
        std::vector<MessageRecord> buffer_;
        std::size_t mask_;
        bool use_mask_;
        alignas(64) std::atomic<types::header::SequenceNumber> write_seq_{0};

        [[nodiscard]]
        std::size_t index_for(types::header::SequenceNumber seq_num) const noexcept;
    };
}
