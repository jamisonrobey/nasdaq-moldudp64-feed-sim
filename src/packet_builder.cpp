#include "packet_builder.h"
#include "mold_udp_64.h"

#include <arpa/inet.h>
#include <cstring>
#include <endian.h>

PacketBuilder::PacketBuilder(std::string_view session)
    : header_{session}
{
}

std::size_t PacketBuilder::size() const
{
    return size_;
}

std::span<const std::byte> PacketBuilder::message_block() const
{
    return std::span{packet_}.subspan(sizeof(MoldUDP64::DownstreamHeader));
}

bool PacketBuilder::empty() const
{
    return header_.msg_count == 0;
}

const std::byte* PacketBuilder::cbegin() const
{
    return packet_.cbegin();
}

const MoldUDP64::Session& PacketBuilder::session() const
{
    return header_.session;
}

std::uint64_t PacketBuilder::seq_num() const
{
    return be64toh(header_.sequence_num);
}

std::uint16_t PacketBuilder::msg_count() const
{
    return header_.msg_count;
}

void PacketBuilder::reset(std::uint64_t seq_num)
{
    header_.sequence_num = htobe64(seq_num);
    header_.msg_count = 0;
    size_ = sizeof(MoldUDP64::DownstreamHeader);
}

bool PacketBuilder::try_add_message(std::span<const std::byte> message)
{
    if (size_ + message.size() > packet_.size())
    {
        return false;
    }

    std::memcpy(&packet_[size_], message.data(), message.size());
    size_ += message.size();
    ++header_.msg_count;
    return true;
}

std::size_t PacketBuilder::finalize()
{
    header_.msg_count = htons(header_.msg_count);
    std::memcpy(packet_.data(), &header_, sizeof(header_));
    return size_;
}
