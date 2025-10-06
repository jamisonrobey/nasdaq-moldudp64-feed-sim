#include "message_buffer.h"

MessageBuffer::MessageBuffer(std::size_t buffer_size)
{
    buffer_.resize(buffer_size);
}

void MessageBuffer::push(std::uint64_t seq, std::size_t pos)
{
    const auto idx{seq % buffer_.size()};

    buffer_[idx].seq_num = seq;
    buffer_[idx].file_pos = pos;

    write_seq_.store(seq, std::memory_order_release);
}

std::optional<std::size_t> MessageBuffer::seq_to_file_pos(std::uint64_t seq)
{
    const auto current_seq{write_seq_.load(std::memory_order_acquire)};

    if (seq > current_seq || seq + buffer_.size() <= current_seq)
    {
        return std::nullopt;
    }

    const auto& entry{buffer_[seq % buffer_.size()]};

    if (entry.seq_num != seq)
    {
        return std::nullopt;
    }

    return entry.file_pos;
}