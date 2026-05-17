#include "imr/mold/packet_builder.h"

#include "../util/binary_io.h"
#include "imr/mold/types.h"

#include <cassert>
#include <source_location>
#include <stdexcept>
#include <format>

namespace
{
    // only one 'slot' technically but used because more clear than msg_count + 1 for full iovec count
    constexpr auto header_slots{1};
}

namespace imr::mold
{
    PacketBuilder::PacketBuilder(const Config& cfg)
        : MTU_{cfg.MTU},
          // resize iovecs for header and worst case where all messages are min size (2kib w/ 1492 MTU)
          iovecs_(header_slots + (MTU_ / cfg.min_message_size))
    {
        if (MTU_ < types::header::length)
        {
            throw std::runtime_error(std::format("{}: Config::MTU must be at least {} bytes",
                                                 std::source_location::current().function_name(), types::header::length));
        }

        if (cfg.session.empty())
        {
            throw std::runtime_error(std::format("{}: Config::session is required",
                                                 std::source_location::current().function_name()));
        }

        if (cfg.session.size() != sizeof(types::header::Session))
        {
            throw std::runtime_error(std::format("{}: Config::session must be exactly 10 characters",
                                                 std::source_location::current().function_name()));
        }

        // write session to header
        util::binary_io::write_at(std::span(header_), 0, cfg.session);

        // iovecs_.front() always header
        iovecs_.front() = iovec{
            .iov_base = header_.data(),
            .iov_len = sizeof(header_),
        };

        min_message_size_ = cfg.min_message_size;
    }

    bool PacketBuilder::try_add(std::span<const char> message) noexcept
    {

        if (bytes_used_ + message.size() > MTU_ || message.empty())
        {
            return false;
        }

        assert(message.size() >= min_message_size_);

        iovecs_[header_slots + message_count_] = iovec{
            .iov_base = const_cast<char*>(message.data()),
            .iov_len = message.size(),
        };

        ++message_count_;
        bytes_used_ += message.size();
        return true;
    }

    std::span<iovec> PacketBuilder::finalize() noexcept
    {
        // write msg count
        util::binary_io::write_at_be(std::span(header_), types::header::message_count_offset, message_count_);

        return std::span(iovecs_.data(), header_slots + message_count_);
    }

    std::string_view PacketBuilder::session() const noexcept
    {
        return std::string_view(header_.data(), sizeof(types::header::Session));
    }

    types::header::MessageCount PacketBuilder::message_count() const noexcept
    {
        return message_count_;
    }

    void PacketBuilder::reset(types::header::SequenceNumber sequence_number) noexcept
    {
        // write seq
        util::binary_io::write_at_be(std::span(header_), types::header::sequence_number_offset, sequence_number);

        // no need to write message_count_ yet
        message_count_ = 0;
        bytes_used_ = types::header::length;
    }
}
