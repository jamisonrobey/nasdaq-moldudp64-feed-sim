#include "imr/mold/downstream/heartbeat.h"

#include "imr/mold/types.h"
#include "imr/util/log.h"
#include "util/binary_io.h"
#include <stop_token>

namespace imr::mold::downstream
{
    Heartbeat::Heartbeat(std::chrono::nanoseconds period,
                         int socket_fd,
                         const sockaddr_in& mcast_group,
                         std::string_view session,
                         const std::atomic<types::header::SequenceNumber>& next_seq)
        : period_{period},
          socket_{socket_fd},
          mcast_group_{mcast_group},
          next_seq_{&next_seq}
    {
        // write header (no need to write seq now)
        std::span packet_span(packet_);
        util::binary_io::write_at(packet_span, types::header::session_offset, session);
        util::binary_io::write_at_be(packet_span, types::header::message_count_offset, types::header::heartbeat_msg_count);

        util::log::debug();
    }

    void Heartbeat::start()
    {
        util::log::info("Hearbeat started");

        thread_ = std::jthread([this](std::stop_token st) {
            while (!st.stop_requested())
            {
                send();
                util::log::debug();

                std::this_thread::sleep_for(period_);
            }
        });
    }

    void Heartbeat::stop()
    {
        thread_.request_stop();

        if (thread_.joinable())
        {
            thread_.join();
        }

        util::log::info("Downstream heartbeat requested stop");
    }

    std::chrono::nanoseconds Heartbeat::period() const noexcept
    {
        return period_;
    }

    void Heartbeat::send() noexcept
    {
        const auto seq{next_seq_->load(std::memory_order_relaxed)};

        util::binary_io::write_at_be(std::span(packet_), types::header::sequence_number_offset, seq);

        if (sendto(socket_,
                   packet_.data(),
                   packet_.size(),
                   0,
                   reinterpret_cast<const sockaddr*>(&mcast_group_),
                   sizeof(mcast_group_)) < 0)
        {
            util::log::perror();
        }
    }

}
