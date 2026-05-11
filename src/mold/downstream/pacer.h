#pragma once
#include <chrono>
#include <concepts>

namespace mold::downstream
{
    template <typename T>
    concept ClockConcept = requires {
        typename T::time_point;
        { T::now() } -> std::same_as<typename T::time_point>;
    };

    template <ClockConcept Clock = std::chrono::high_resolution_clock>
    class Pacer
    {
      public:
        Pacer(double playback_speed, std::chrono::nanoseconds ignore_packets_before, Clock::time_point replay_wall_start)
            : playback_speed_{playback_speed},
              ignore_packets_before_{ignore_packets_before},
              replay_wall_start_{replay_wall_start} {}

        std::optional<std::chrono::nanoseconds> get_delay(std::chrono::nanoseconds packet_timestamp)
        {
            if (packet_timestamp < ignore_packets_before_)
            {
                return std::nullopt;
            }

            [[unlikely]]
            if (!first_packet_timestamp_.has_value())
            {
                first_packet_timestamp_ = packet_timestamp;
                return std::chrono::nanoseconds{0};
            }

            const auto message_time_offset{packet_timestamp - *first_packet_timestamp_};
            const auto wall_time_offset{std::chrono::nanoseconds(static_cast<int64_t>(message_time_offset.count() / playback_speed_))};

            const auto scheduled_dispatch_time{replay_wall_start_ + wall_time_offset};
            const auto now{Clock::now()};

            if (scheduled_dispatch_time <= now)
            {
                return std::chrono::nanoseconds{0};
            }

            return std::chrono::duration_cast<std::chrono::nanoseconds>(scheduled_dispatch_time - now);
        }

      private:
        double playback_speed_;
        std::chrono::nanoseconds ignore_packets_before_;
        Clock::time_point replay_wall_start_;
        std::optional<std::chrono::nanoseconds> first_packet_timestamp_;
    };
}
