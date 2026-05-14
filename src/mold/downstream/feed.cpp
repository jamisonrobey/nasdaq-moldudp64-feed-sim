#include "imr/mold/downstream/feed.h"

#include "../io.h"
#include "../../itch/timestamp.h"

#include <cassert>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <print>

namespace imr::mold::downstream
{
    Feed::Feed(const Config& cfg, const PacketBuilder::Config& packet_builder_cfg, std::span<const char> file, RetransmissionBuffer& retransmission_buffer)
        : socket_{socket(AF_INET, SOCK_DGRAM, 0)},
          pacer_{cfg.pacer_cfg},
          file_{file},
          packet_builder_{packet_builder_cfg},
          retransmission_buffer_{&retransmission_buffer}
    {
        setup_networking(cfg);
    }

    void Feed::start()
    {
        while (file_pos_ < file_.size() && !should_stop_)
        {
            const std::chrono::nanoseconds timestamp{fill_packet()};
            flush_packet(timestamp);
        }
    }

    void Feed::stop()
    {
        should_stop_ = true;
    }

    void Feed::reset(std::span<const char> new_file)
    {
        if (!new_file.empty())
        {
            file_ = new_file;
        }
        file_pos_ = 0;
        sequence_number_ = 1;
    }

    std::chrono::nanoseconds Feed::fill_packet()
    {
        static constexpr auto first_msg_timestamp_not_set_sentinel{-1};

        packet_builder_.reset(sequence_number_);

        std::chrono::nanoseconds first_msg_timestamp{first_msg_timestamp_not_set_sentinel};

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

            // get timestamp of first msg for replay
            if (first_msg_timestamp.count() == first_msg_timestamp_not_set_sentinel)
            {
                // itch::extract_timestamp doesn't know about len prefix so subspan skips over
                first_msg_timestamp = itch::extract_timestamp(msg.subspan(sizeof(LengthPrefix)));
            }

            assert(retransmission_buffer_ != nullptr);
            retransmission_buffer_->push({
                .sequence_number = sequence_number_++,
                .file_position = msg_file_pos,
            });
        }

        return first_msg_timestamp;
    }

    void Feed::flush_packet(std::chrono::nanoseconds first_msg_timestamp)
    {
        handle_delay(first_msg_timestamp);
        send(packet_builder_.finalize());
    }

    void Feed::send([[maybe_unused]] std::span<const char> msg)
    {
#ifndef DEBUG_NO_NETWORK
        if (const auto bytes_sent{sendto(socket_.get(),
                                         msg.data(),
                                         msg.size(),
                                         0,
                                         reinterpret_cast<sockaddr*>(&mcast_addr_in_),
                                         sizeof(mcast_addr_in_))};
            bytes_sent < 0)
        {
            std::perror("sendto");
        }
        else if (static_cast<std::size_t>(bytes_sent) != msg.size())
        {
            std::println(stderr, "sendto: only sent {} of {} bytes", bytes_sent, msg.size());
        }
#endif
    }

    void Feed::handle_delay(std::chrono::nanoseconds first_msg_timestamp)
    {
        if ([[maybe_unused]] const std::optional delay{pacer_.get_delay(first_msg_timestamp)}; delay.has_value())
        {
#ifndef DEBUG_NO_SLEEP
            std::this_thread::sleep_for(*delay);
#endif
        }
    }

    void Feed::setup_networking(const Config& cfg)
    {
        constexpr auto sockopt_on{1};
        if (setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0 ||
            setsockopt(socket_.get(), IPPROTO_IP, IP_MULTICAST_TTL, &cfg.ttl, sizeof(cfg.ttl)) < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        const auto loopback_opt{static_cast<int>(cfg.loopback)};
        if (setsockopt(socket_.get(), IPPROTO_IP, IP_MULTICAST_LOOP, &loopback_opt, sizeof(loopback_opt)) < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        mcast_addr_in_.sin_family = AF_INET;
        mcast_addr_in_.sin_port = std::byteswap(cfg.port);

        // probably cant use string_view here mabye write minimal zstring_view????
        if (const auto ret{inet_pton(AF_INET, cfg.mcast_group.data(), &mcast_addr_in_.sin_addr)}; ret == 0)
        {
            throw std::invalid_argument(std::format("invalid ip format for downstream group {}", cfg.mcast_group));
        }
        else if (ret < 0)
        {
            throw std::system_error(errno, std::system_category());
        }
    }
}
