#include "imr/mold/packet_builder.h"

#include "../util/binary_io.h"
#include "imr/mold/types.h"

#include <cassert>
#include <print>
#include <source_location>
#include <stdexcept>
#include <format>

namespace imr::mold
{
    PacketBuilder::PacketBuilder(const Config& cfg)
        : MTU_{cfg.MTU}
    {
        if (MTU_ < types::header::length)
        {
            throw std::invalid_argument(std::format("{}: Config::MTU must be at least {} bytes",
                                                    std::source_location::current().function_name(), types::header::length));
        }

        if (cfg.session.size() != sizeof(types::header::Session))
        {
            throw std::invalid_argument(std::format("{}: Config::session must be exactly 10 characters",
                                                    std::source_location::current().function_name()));
        }

        // + 1 for header
        iovecs_.reserve(1 + (MTU_ / min_message_size));

        util::binary_io::write_at(std::span(header_buffer_), 0, cfg.session);
        iovecs_.emplace_back(header_buffer_.data(), header_buffer_.size());
        bytes_remaining_ = MTU_ - types::header::length;
    }

    bool PacketBuilder::try_add(std::span<const char> message) noexcept
    {
        if (message.size() > bytes_remaining_ || message.empty())
        {
            return false;
        }

        assert(message.size() >= min_message_size);

        iovecs_.emplace_back(const_cast<char*>(message.data()), message.size());
        bytes_remaining_ -= message.size();

        return true;
    }

    std::span<iovec> PacketBuilder::finalize() noexcept
    {
        util::binary_io::write_at_be(std::span(header_buffer_), types::header::message_count_offset, message_count());
        return iovecs_;
    }

    void PacketBuilder::reset(types::header::SequenceNumber seq) noexcept
    {
        util::binary_io::write_at_be(std::span(header_buffer_), types::header::sequence_number_offset, seq);
        iovecs_.resize(1); // clear messages (iovecs_[1..])
        bytes_remaining_ = MTU_ - types::header::length;
    }

    types::header::MessageCount PacketBuilder::message_count() const noexcept
    {
        return static_cast<types::header::MessageCount>(iovecs_.size() - 1);
    }

    std::string_view PacketBuilder::session() const noexcept
    {
        return std::string_view(header_buffer_.data(), sizeof(types::header::Session));
    }
}
