#include "message_buffer.h"

#include <stdexcept>
#include <cassert>

MessageBuffer::MessageBuffer(std::size_t buffer_size)
{
    if ((buffer_size & (buffer_size - 1)) != 0 || buffer_size == 0)
    {
        throw std::invalid_argument("MessageBuffer is designed to be used with buffer_size that is a pow of 2");
    }
    buffer_.resize(buffer_size);
}

// pre: called from one thread with unique, monotonically increasing sequence numbers

void MessageBuffer::push(Message msg)
{
#ifndef NDEBUG
    // todo: this is slow asf (even for debug) convert to static local cache
    for (const auto& [seq, _] : buffer_)
    {
        assert(msg.seq_num != seq && "Duplicate sequence was added to buffer");
    }
#endif

    const auto idx{msg.seq_num % buffer_.size()};

    buffer_[idx].seq_num = msg.seq_num;
    buffer_[idx].file_pos = msg.file_pos;

    write_seq_.store(msg.seq_num, std::memory_order_release);
}

// pre: all sequence nums are unique

std::optional<std::size_t> MessageBuffer::get_file_pos_for_seq(std::uint64_t seq_num)
{
    const auto current_seq{write_seq_.load(std::memory_order_acquire)};

    if (seq_num + buffer_.size() <= current_seq ||
        seq_num > current_seq ||
        current_seq == 0) // empty
    {
        return std::nullopt;
    }

    const auto& entry{buffer_[seq_num % buffer_.size()]};

    if (entry.seq_num != seq_num)
    {
        return std::nullopt;
    }

    return entry.file_pos;
}

std::size_t MessageBuffer::size() const
{
    return buffer_.size();
}