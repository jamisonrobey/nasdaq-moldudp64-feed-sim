#include "imr/mold/retransmission_buffer.h"

#include <print>
#include <source_location>
#include <stdexcept>

namespace imr::mold
{

    RetransmissionBuffer::RetransmissionBuffer(std::size_t buffer_size)
        : mask_{buffer_size - 1},
          use_mask_{buffer_size != 0 && (buffer_size & mask_) == 0}
    {
        if (buffer_size == 0)
        {
            throw std::invalid_argument("mold::RetransmissionBuffer: buffer_size must be > 0");
        }

        if (!use_mask_)
        {
            std::println("{}: non power-of-two buffer_size ({}) will reduce retransmission buffer performance",
                         std::source_location::current().function_name(),
                         buffer_size);
        }

        buffer_.resize(buffer_size);
    }

    void RetransmissionBuffer::push(RetransmissionBuffer::MessageRecord&& message_record) noexcept
    {
        buffer_[index_for(message_record.sequence_number)] = message_record;

        write_seq_.store(message_record.sequence_number, std::memory_order_release);
    }

    std::optional<SequenceNumber> RetransmissionBuffer::file_position_for(SequenceNumber sequence_number)
        const noexcept
    {
        const auto current_sequence_number{write_seq_.load(std::memory_order_acquire)};

        const auto overwritten{sequence_number + buffer_.size() <= current_sequence_number};
        const auto not_yet_sent{sequence_number > current_sequence_number};
        const auto buffer_empty{current_sequence_number == 0};

        if (overwritten || not_yet_sent || buffer_empty)
        {
            return std::nullopt;
        }

        const auto& entry{buffer_[index_for(sequence_number)]};

        // check if writer lapped us between checking overwritten above and this read
        if (entry.sequence_number != sequence_number)
        {
            return std::nullopt;
        }

        return entry.file_position;
    }

    std::size_t RetransmissionBuffer::index_for(SequenceNumber sequence_number) const noexcept
    {
        return use_mask_ ? sequence_number & mask_
                         : sequence_number % buffer_.size();
    }

    std::size_t RetransmissionBuffer::size() const noexcept
    {
        return buffer_.size();
    }
}
