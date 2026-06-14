#pragma once

#include "imr/mold/types.h"

#include <atomic>
#include <chrono>
#include <netinet/in.h>
#include <stop_token>
#include <thread>

namespace imr::mold::downstream
{
    class Heartbeat
    {
      public:
        Heartbeat(std::chrono::nanoseconds period,
                  int socket_fd,
                  const sockaddr_in& mcast_group,
                  std::string_view session,
                  const std::atomic<types::header::SequenceNumber>& next_seq);

        void start(std::stop_token st);

        [[nodiscard]]
        std::chrono::nanoseconds period() const noexcept;

      private:
        std::array<char, types::header::length> packet_{};
        std::chrono::nanoseconds period_;
        int socket_;
        sockaddr_in mcast_group_;
        const std::atomic<types::header::SequenceNumber>* next_seq_;
        std::jthread thread_;

        void send() noexcept;
    };
}
