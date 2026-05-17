#include "imr/mold/downstream/feed.h"

#include "../io.h"
#include "../../itch/timestamp.h"

#include <cassert>
#include <source_location>
#include <arpa/inet.h>
#include <thread>
#include <print>

namespace imr::mold::downstream
{
    Feed::Feed(const Config& cfg, const PacketBuilder::Config& packet_builder_cfg, std::span<const char> file, RetransmissionBuffer& retransmission_buffer)
        : socket_{socket(AF_INET, SOCK_DGRAM, 0)},
          file_{file},
          pacer_{cfg.pacer_cfg},
          packet_builder_{packet_builder_cfg},
          retransmission_buffer_{&retransmission_buffer}
    {
        configure_socket(cfg);
    }

    void Feed::start()
    {
        while (file_pos_ < file_.size() && !stop_requested_)
        {
            apply_pacing(build_packet());
            send_packet();
        }
    }

    void Feed::stop() noexcept
    {
        stop_requested_ = true;
    }

    std::chrono::nanoseconds Feed::build_packet()
    {
        packet_builder_.reset(sequence_number_);

        auto first_message{true};
        Timestamp first_msg_timestamp;

        while (file_pos_ < file_.size())
        {
            const std::size_t msg_file_pos{file_pos_};
            const std::span msg{mold::io::read_message(file_, file_pos_)};

            // eof
            if (msg.empty()) [[unlikely]]
            {
                break;
            }

            // packet full
            if (!packet_builder_.try_add(msg))
            {
                file_pos_ = msg_file_pos;
                break;
            }

            if (first_message)
            {
                first_msg_timestamp = itch::extract_timestamp(msg.subspan(sizeof(types::LengthPrefix)));
                first_message = false;
            }

            assert(retransmission_buffer_ != nullptr);
            retransmission_buffer_->push({
                .sequence_number = sequence_number_++,
                .file_position = msg_file_pos,
            });
        }

        return first_msg_timestamp;
    }

    void Feed::apply_pacing(Timestamp first_msg_timestamp)
    {
        if ([[maybe_unused]] const std::optional delay{pacer_.get_delay(first_msg_timestamp)}; delay.has_value())
        {
#ifndef DEBUG_NO_SLEEP
            std::this_thread::sleep_for(*delay);
#endif
        }
    }

    void Feed::send_packet() noexcept
    {
        [[maybe_unused]]
        const std::span packet{packet_builder_.finalize()};

#ifndef DEBUG_NO_NETWORK
        send_hdr_.msg_iov = packet.data();
        send_hdr_.msg_iovlen = packet.size();

        if (const auto bytes_sent{sendmsg(socket_.get(), &send_hdr_, 0)}; bytes_sent < 0)
        {
            std::perror("sendto");
        }
        else if (static_cast<std::size_t>(bytes_sent) != packet.size())
        {
            std::println(stderr, "sendto: only sent {} of {} bytes", bytes_sent, packet.size());
        }
#endif
    }

    void Feed::configure_socket(const Config& cfg)
    {
        constexpr auto sockopt_on{1};
        if (setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0 ||
            setsockopt(socket_.get(), IPPROTO_IP, IP_MULTICAST_TTL, &cfg.ttl, sizeof(cfg.ttl)) < 0)
        {
            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }

        const auto loopback_opt{static_cast<int>(cfg.loopback)};
        if (setsockopt(socket_.get(), IPPROTO_IP, IP_MULTICAST_LOOP, &loopback_opt, sizeof(loopback_opt)) < 0)
        {

            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }

        dest_addr_.sin_family = AF_INET;
        dest_addr_.sin_port = std::byteswap(cfg.port);

        if (const auto ret{inet_pton(AF_INET, cfg.mcast_group.c_str(), &dest_addr_.sin_addr)}; ret == 0)
        {
            throw std::invalid_argument(std::format("invalid ip format for downstream group {}", cfg.mcast_group.c_str()));
        }
        else if (ret < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        send_hdr_.msg_name = &dest_addr_;
        send_hdr_.msg_namelen = sizeof(dest_addr_);
    }
}
