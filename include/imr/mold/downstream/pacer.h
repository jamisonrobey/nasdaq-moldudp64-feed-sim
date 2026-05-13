#pragma once

#include <chrono>
#include <concepts>

namespace imr::mold::downstream
{
    enum class MarketPhase
    {
        pre,
        close,
        open
    };

    template <typename T>
    concept ClockConcept = requires {
        typename T::time_point;
        { T::now() } -> std::same_as<typename T::time_point>;
    };

    template <ClockConcept Clock = std::chrono::steady_clock>
    class Pacer
    {
      public:
        struct Config
        {
            double playback_speed{1.0};
            // MarketPhase::pre means no packets ignored
            MarketPhase ignore_packets_before_phase{MarketPhase::pre};
            Clock::time_point playback_wall_start{Clock::now()};
        };

        Pacer(const Config& cfg)
            : playback_speed_{cfg.playback_speed},
              ignore_packets_before_{phase_timestamp(cfg.ignore_packets_before_phase)},
              replay_wall_start_{cfg.playback_wall_start} {}

        [[nodiscard]]
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

            const auto wall_time_offset{
                std::chrono::nanoseconds(
                    static_cast<int64_t>(static_cast<double>(message_time_offset.count()) / playback_speed_)),
            };

            const auto scheduled_dispatch_time{replay_wall_start_ + wall_time_offset};
            const auto now{Clock::now()};

            if (scheduled_dispatch_time <= now)
            {
                return std::chrono::nanoseconds{0};
            }

            return std::chrono::duration_cast<std::chrono::nanoseconds>(scheduled_dispatch_time - now);
        }

        static constexpr std::chrono::nanoseconds phase_timestamp(MarketPhase phase)
        {
            using namespace std::chrono_literals;
            switch (phase)
            {
            case MarketPhase::pre:
                return 0ns;
            case MarketPhase::open:
                return 9h + 30min;
            case MarketPhase::close:
                return 16h;
            default:
                return 0ns;
            }
        }

      private:
        double playback_speed_;
        std::chrono::nanoseconds ignore_packets_before_;
        Clock::time_point replay_wall_start_;
        std::optional<std::chrono::nanoseconds> first_packet_timestamp_;
    };
}
