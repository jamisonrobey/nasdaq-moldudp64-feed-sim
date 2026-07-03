#pragma once

#include "imr/mold/types.h"
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <sys/uio.h>
#include <vector>

namespace imr::mold
{
    /** Builds MoldUDP64 packets.
     *
     *  Usage:
     *
     *  1. Call reset() to write the sequence number to the header.
     *
     *  2. Add messages with try_add() until all are added, or it returns false (packet full per the configured MTU).
     *
     *  3. Call finalize() to write the message count to the header and get the completed packet as a span of iovecs
     */
    class PacketBuilder
    {
      public:
        // todo change this to something like totalview_5_0_min_message_size and update tests but just leaving now to make tests compile temporarily
        static constexpr auto min_message_size{14};
        struct Config
        {
            /// MoldUDP64 Session, Must be 10 characters.
            std::string_view session;
            /// MTU for packet, default is ethernet MTU (1500 bytes) - IP/UDP headers (28 bytes).
            std::size_t MTU{1472};
            /**  Smallest possible message size (including 2-byte length prefix); defaults to 14 (TotalView-ITCH 5.0).
             *
             * Configurable in case a future TotalView version introduces smaller messages.
             *
             * Used to size the iovecs vector for the worst case packet (`MTU` / `min_message_size`),
             * i.e. a packet filled entirely with minimum-size messages.
             *
             @see https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/nqtvitchspecification.pdf
            */
            std::size_t min_message_size{PacketBuilder::min_message_size};
        };

        /**
         *  @throws std::invalid_argument if cfg.session is not 10 characters
         *  @throws std::invalid_argument if cfg.MTU less than the size of a MoldUDP64 header (20 bytes)
         */
        explicit PacketBuilder(const Config& cfg);
        /** Adds a message to the messge block.
         *  
         *  @returns false without adding message if wouldn't fit under configured MTU.
         */
        [[nodiscard]]
        bool try_add(std::span<const char> message) noexcept;
        /// Writes the message count to the header and returns header + message block as iovecs.
        [[nodiscard]]
        std::span<iovec> finalize() noexcept;
        /// Writes sequence number for new packet to header and clears the message block.
        void reset(types::header::SequenceNumber seq = 1) noexcept;

        /// Number of messages added since last `reset()`.
        [[nodiscard]]
        types::header::MessageCount message_count() const noexcept;
        /// Returns the configured MoldUDP64 session from header
        [[nodiscard]]
        std::string_view session() const noexcept;

      private:
        std::size_t MTU_;
        std::size_t bytes_remaining_;

        std::array<char, types::header::length> header_buffer_{};
        // [0] = header (*iov_base = header_buffer_), [1..N] = messages
        // N = 1 (header) + (MTU_ / min_message_size_)
        std::vector<iovec> iovecs_;

        std::size_t min_message_size_;
    };
}
