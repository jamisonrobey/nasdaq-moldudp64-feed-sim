#include "imr/mold/packet_builder.h"

#include "../util/binary_io.h"

#include <cassert>
#include <stdexcept>

namespace
{
    constexpr auto session_str_size{10UZ};
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

        if (cfg.session.size() != session_str_size)
        {
            throw std::runtime_error("PacketBuilder: A MoldUDP64 session must be exactly 10 characters");
        }

        util::binary_io::write(buffer_, write_pos_, cfg.session);

        reset(0);
    }

    void PacketBuilder::reset(mold::SequenceNumber sequence_number)
    {
        // session never changed so start write_pos_ after
        write_pos_ = session_str_size;

        util::binary_io::write_be(buffer_, write_pos_, sequence_number);

        msg_count_ = 0;
        // no need to write msg_count_ now
        write_pos_ += sizeof(msg_count_);
    }

    bool PacketBuilder::try_add(std::span<const char> message)
    {
        if (write_pos_ + message.size() > MTU_ || message.size() == 0)
        {
            return false;
        }

        util::binary_io::write(buffer_, write_pos_, message);
        ++msg_count_;

        return true;
    }

    std::span<const char> PacketBuilder::finalize()
    {
        util::binary_io::write_at_be(buffer_, msg_count_offset, msg_count_);
        return std::span(buffer_.data(), write_pos_);
    }

    mold::MessageCount PacketBuilder::message_count() const noexcept
    {
        return msg_count_;
    }
}
