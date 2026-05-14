#include "imr/mold/packet_builder.h"

#include "../util/binary_io.h"
#include "imr/mold/types.h"

#include <cassert>
#include <stdexcept>

namespace
{
    constexpr auto session_str_size{10UZ};
    constexpr auto sequence_number_offset{session_str_size};
    constexpr auto msg_count_offset{session_str_size + sizeof(imr::mold::SequenceNumber)};
}

namespace imr::mold
{
    PacketBuilder::PacketBuilder(const Config& cfg)
        : MTU_{cfg.MTU},
          buffer_(MTU_)
    {
        if (MTU_ < header_length)
        {
            throw std::runtime_error("PacketBuilder: MTU was must be at least {} bytes");
        }

        if (cfg.session.empty())
        {
            throw std::runtime_error("PacketBuilder: session is required");
        }

        if (cfg.session.size() != session_str_size)
        {
            throw std::runtime_error("PacketBuilder: A MoldUDP64 session must be exactly 10 characters");
        }

        util::binary_io::write(buffer_, write_pos_, cfg.session);
    }

    bool PacketBuilder::try_add(std::span<const char> message) noexcept
    {
        if (write_pos_ + message.size() > MTU_ || message.size() == 0)
        {
            return false;
        }

        util::binary_io::write(buffer_, write_pos_, message);
        ++msg_count_;

        return true;
    }

    std::span<const char> PacketBuilder::finalize() noexcept
    {
        util::binary_io::write_at_be(buffer_, msg_count_offset, msg_count_);

        // not required to be noexcept by the standard, but is in gcc/clang (supported compilers)
        static_assert(noexcept(std::span<const char>(static_cast<const char*>(nullptr), 0uz)),
                      "span(ptr, count) must be implemented as noexcept");
        return std::span(buffer_.data(), write_pos_);
    }

    void PacketBuilder::reset(mold::SequenceNumber sequence_number) noexcept
    {
        // session never changes so step over
        write_pos_ = sequence_number_offset;

        util::binary_io::write_be(buffer_, write_pos_, sequence_number);

        msg_count_ = 0;
        // no need to write msg_count_ now
        write_pos_ += sizeof(msg_count_);
    }

    void PacketBuilder::set_sequence_number(SequenceNumber sequence_number) noexcept
    {
        util::binary_io::write_at_be(buffer_, sequence_number_offset, sequence_number);
    }

    mold::MessageCount PacketBuilder::message_count() const noexcept
    {
        return msg_count_;
    }

    void PacketBuilder::set_message_count(MessageCount msg_count) noexcept
    {
        util::binary_io::write_at_be(buffer_, msg_count_offset, msg_count);
    }

}
