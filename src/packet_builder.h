#ifndef PACKET_BUILDER_H
#define PACKET_BUILDER_H

#include "mold_udp_64.h"

class PacketBuilder
{
  public:
    explicit PacketBuilder(std::string_view session);

    [[nodiscard]]
    bool empty() const;

    [[nodiscard]]
    std::size_t size() const;

    [[nodiscard]]
    const std::byte* cbegin() const;

    [[nodiscard]]
    const MoldUDP64::Session& session() const;

    [[nodiscard]]
    std::uint64_t seq_num() const;

    [[nodiscard]]
    std::uint16_t msg_count() const;

    [[nodiscard]]
    std::span<const std::byte> message_block() const;

    void reset(std::uint64_t mold_seq_num);

    bool try_add_message(std::span<const std::byte> message);

    std::size_t finalize();

  private:
    MoldUDP64::Packet packet_{};
    std::size_t size_{sizeof(MoldUDP64::DownstreamHeader)};
    MoldUDP64::DownstreamHeader header_;
};

#endif
