#include "retransmission_buffer.h"
#include "mold/types.h"

#include <format>
#include <stdexcept>

namespace mold
{
    RetransmissionBuffer::RetransmissionBuffer(std::size_t buffer_size)
        : mask_{buffer_size - 1}
    {
        if ((buffer_size & mask_) != 0 || buffer_size == 0)
        {
            throw std::invalid_argument(std::format("mold::RetransmissionBuffer: buffer_size must be a power of 2 (given  {})",
                                                    buffer_size));
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

        // check if slot has been overwritten / lapped
        if (entry.sequence_number != sequence_number)
        {
            return std::nullopt;
        }

        return entry.file_position;
    }

    std::size_t RetransmissionBuffer::index_for(SequenceNumber sequence_number) const noexcept
    {
        return sequence_number & mask_;
    }

    std::size_t RetransmissionBuffer::size() const noexcept
    {
        return buffer_.size();
    }
}
