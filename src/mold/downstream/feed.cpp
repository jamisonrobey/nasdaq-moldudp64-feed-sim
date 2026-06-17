#include "imr/mold/downstream/feed.h"

#include "../io.h"
#include "../../itch/timestamp.h"
#include "imr/mold/types.h"
#include "util/binary_io.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <netinet/in.h>
#include <source_location>
#include <arpa/inet.h>
#include <stop_token>
#include <thread>
#include <print>
#include <format>

namespace
{
    std::optional<std::chrono::nanoseconds> peek_timestamp(std::span<const char> bytes)
    {
        using namespace imr;

        if (bytes.size() < sizeof(mold::types::LengthPrefix) + itch::timestamp_size) [[unlikely]]
        {
            return std::nullopt;
        }

        return itch::extract_timestamp(bytes.subspan(sizeof(mold::types::LengthPrefix)));
    }
}

namespace imr::mold::downstream
{
    Feed::Feed(const Config& cfg, const PacketBuilder::Config& packet_builder_cfg, std::span<const char> file, RetransmissionBuffer& retransmission_buffer)
        : socket_{socket(AF_INET, SOCK_DGRAM, 0)},
          mcast_group_{configure_socket(cfg)},
          file_(file),
          retransmission_buffer_(&retransmission_buffer),
          pacer_(cfg.pacer_cfg),
          packet_builder_{packet_builder_cfg},
          heartbeat_(cfg.heartbeat_period, socket_.get(), mcast_group_, packet_builder_cfg.session, sent_sequence_number_),
          end_of_session_duration_{cfg.end_of_session_duration}
    {
        send_hdr_.msg_name = &mcast_group_;
        send_hdr_.msg_namelen = sizeof(mcast_group_);
    }

    void Feed::start(std::stop_token st)
    {
        heartbeat_.start(st);

        while (file_pos_ < file_.size() && !st.stop_requested())
        {
            std::optional timestamp{peek_timestamp(file_.subspan(file_pos_))};

            if (!timestamp.has_value())
            {
                break;
            }

            if (pacer_.should_skip(*timestamp))
            {
                // eof / malformed
                if (!io::skip_message(file_, file_pos_))
                {
                    break;
                }

                continue;
            }

            build_packet();

            if (const std::optional delay{pacer_.get_delay(*timestamp)}; delay.has_value() && delay->count() != 0)
            {
#ifndef DEBUG_NO_SLEEP
                std::this_thread::sleep_for(*delay);
#endif
            }

            send_packet();
        }

        end_of_session(st);
    }

    void Feed::build_packet()
    {
        packet_builder_.reset(sequence_number_);

        while (file_pos_ < file_.size())
        {

            const std::size_t msg_file_pos{file_pos_};

            const std::span msg{mold::io::read_message(file_, file_pos_)};

            if (msg.empty()) [[unlikely]]
            {
                return;
            }

            // rollback when packet is full
            if (!packet_builder_.try_add(msg))
            {
                file_pos_ = msg_file_pos;
                return;
            }

            assert(retransmission_buffer_ != nullptr);
            retransmission_buffer_->push({
                .sequence_number = sequence_number_++,
                .file_position = msg_file_pos,
            });
        }
    }

    void Feed::send_packet() noexcept
    {

        [[maybe_unused]]
        const std::span packet{packet_builder_.finalize()};

        sent_sequence_number_.store(sequence_number_, std::memory_order_relaxed);

#ifndef DEBUG_NO_NETWORK
        send_hdr_.msg_iov = packet.data();
        send_hdr_.msg_iovlen = packet.size();

        if (const auto bytes_sent{sendmsg(socket_.get(), &send_hdr_, 0)}; bytes_sent < 0)
        {
            std::perror("downstream sendto");
        }
        // todo: fix this condition because iovec size doesnt = the packet sent size
        else if (static_cast<std::size_t>(bytes_sent) != packet.size())
        {
            // std::println(stderr, "sendto: only sent {} of {} bytes", bytes_sent, packet.size());
        }
#endif
    }

    void Feed::end_of_session([[maybe_unused]] std::stop_token st)
    {
#ifndef DEBUG_NO_NETWORK
        std::array<char, types::header::length> eos_packet{};

        std::size_t pos{0};
        util::binary_io::write_at(std::span(eos_packet), pos, packet_builder_.session());

        static constexpr types::header::MessageCount eos_msg_count{std::numeric_limits<types::header::MessageCount>::max()};
        util::binary_io::write_at_be(std::span(eos_packet), pos, eos_msg_count);

        util::binary_io::write_at_be(std::span(eos_packet), pos, sequence_number_);

        const auto start{std::chrono::high_resolution_clock::now()};
        const auto end{start + end_of_session_duration_};

        while (!st.stop_requested() && end > std::chrono::high_resolution_clock::now())
        {
            if (const auto bytes_sent{sendto(socket_.get(),
                                             eos_packet.data(),
                                             sizeof(eos_packet),
                                             0,
                                             reinterpret_cast<sockaddr*>(&mcast_group_),
                                             sizeof(mcast_group_))};
                bytes_sent < 0)
            {
                std::perror("downstream sendto");
            }
            else if (static_cast<std::size_t>(bytes_sent) < eos_packet.size())
            {
                std::println(stderr, "sendto: only sent {} of {} bytes", bytes_sent, eos_packet.size());
            }

#ifndef DEBUG_NO_SLEEP
            std::this_thread::sleep_for(heartbeat_.period());
#endif
        }
#endif
    }

    sockaddr_in Feed::configure_socket([[maybe_unused]] const Config& cfg) const
    {
        constexpr auto sockopt_on{1};
        if (setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0 ||
            setsockopt(socket_.get(), IPPROTO_IP, IP_MULTICAST_TTL, &cfg.ttl, sizeof(cfg.ttl)) < 0)
        {
            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }

        sockaddr_in mcast_group{};
        mcast_group.sin_family = AF_INET;
        mcast_group.sin_port = std::byteswap(cfg.port);

        if (const auto ret{inet_pton(AF_INET, cfg.mcast_group.c_str(), &mcast_group.sin_addr)}; ret == 0)
        {
            throw std::invalid_argument(std::format("invalid ip format for downstream group {}", cfg.mcast_group.c_str()));
        }
        else if (ret < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        if (!cfg.loopback)
        {
            return mcast_group;
        }

        if (setsockopt(socket_.get(), IPPROTO_IP, IP_MULTICAST_LOOP, &sockopt_on, sizeof(sockopt_on)) < 0)
        {
            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }

        return mcast_group;
    }
}
