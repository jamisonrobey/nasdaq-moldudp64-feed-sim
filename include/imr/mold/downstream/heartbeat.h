#pragma once

#include "imr/mold/types.h"
#include "imr/util/file_descriptor.h"

#include <atomic>
#include <chrono>
#include <netinet/in.h>
#include <thread>

namespace imr::mold::downstream
{
    /// Sends MoldUDP64 heartbeat packets at a configured interval
    class Heartbeat
    {
      public:
        /**
         @param period      Interval between heartbeat packets.
         @param socket      UDP socket to send on
         @param next_seq    Sequence number read on each send; must outlive this object.
        */
        Heartbeat(std::chrono::nanoseconds period,
                  util::FileDescriptor& socket,
                  const sockaddr_in& mcast_group,
                  std::string_view session,
                  const std::atomic<types::header::SequenceNumber>& next_seq);

        void start();

        void stop();

        [[nodiscard]]
        std::chrono::nanoseconds period() const noexcept;

      private:
        std::array<char, types::header::length> packet_{};
        std::chrono::nanoseconds period_;
        util::FileDescriptor* socket_;

        sockaddr_in mcast_group_;
        const std::atomic<types::header::SequenceNumber>* next_seq_;
        std::jthread thread_;

        void send() noexcept;
    };
}
