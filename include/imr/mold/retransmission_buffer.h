#pragma once
#include "imr/mold/types.h"
#include <vector>
#include <atomic>
#include <cstddef>
#include <optional>
namespace imr::mold
{
    /** Retransmission buffer of the last `buffer_size` messages.
     *
     *  Implemented as a circular array, using the sequence number as the index. Single writer (downstream),
     *  N readers (retransmission); acquire/release on write_seq_ guards buffer visibility across threads.
     */
    class RetransmissionBuffer
    {
      public:
        /**
         *  @param buffer_size size of the retransmission buffer, in messages. Choose a power of two for faster
         *  lookup via bitmasking; non power-of-two sizes fall back to division.
         *
         *  @throws std::invalid_argument if buffer_size is 0.
         */
        explicit RetransmissionBuffer(std::size_t buffer_size);

        struct MessageRecord
        {
            types::header::SequenceNumber sequence_number;
            std::size_t file_position;
        };

        /// Records a message's file position under its sequence number, overwriting the oldest entry if the buffer is full.
        void push(const MessageRecord& message_record) noexcept;

        /// Returns the file position for seq_num, or std::nullopt if it's not currently in the buffer.
        [[nodiscard]]
        std::optional<std::size_t> file_position_for(types::header::SequenceNumber seq_num) const noexcept;

        /// Capacity of the buffer, in messages.
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
