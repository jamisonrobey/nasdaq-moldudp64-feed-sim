#pragma once

#include "imr/util/log.h"
#include <chrono>
#include <concepts>

#include <print>

namespace imr::mold::downstream
{
    enum class MarketPhase
    {
        pre,
        open,
        close
    };

    inline constexpr std::chrono::nanoseconds phase_to_ns(MarketPhase phase)
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

    // less verbose for caller than imr::mold::downstream::phase_to_ns(imr::mold::downstream::MarketPhase::pre)
    inline constexpr auto market_open{phase_to_ns(MarketPhase::open)};
    inline constexpr auto market_pre{phase_to_ns(MarketPhase::pre)};
    inline constexpr auto market_close{phase_to_ns(MarketPhase::close)};

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
            std::chrono::nanoseconds skip_before{phase_to_ns(MarketPhase::pre)};
        };

        Pacer(const Config& cfg)
            : playback_speed_{cfg.playback_speed},
              skip_before_{cfg.skip_before} {}

        [[nodiscard]]
        bool should_skip(std::chrono::nanoseconds packet_timestamp)
        {
            return packet_timestamp < skip_before_;
        }

        [[nodiscard]]
        std::optional<std::chrono::nanoseconds> get_delay(std::chrono::nanoseconds packet_timestamp)
        {
            if (should_skip(packet_timestamp))
            {
                return std::nullopt;
            }

            if (!replay_origin_.has_value())

            {
                replay_origin_ = packet_timestamp;
                wall_origin_ = Clock::now();

                util::log::debug("Downstream pacer: replay origin set at {}ns (wall origin {}ns since epoch)",
                                 replay_origin_->count(),
                                 wall_origin_.time_since_epoch().count());
            }

            return calculate_delay(packet_timestamp);
        }

      private:
        double playback_speed_;
        std::chrono::nanoseconds skip_before_;
        Clock::time_point wall_origin_;
        std::optional<std::chrono::nanoseconds> replay_origin_;

        [[nodiscard]]
        std::chrono::nanoseconds calculate_delay(std::chrono::nanoseconds packet_timestamp)
        {
            const auto replay_offset{packet_timestamp - *replay_origin_};

            const auto scaled_offset{
                std::chrono::nanoseconds(
                    static_cast<int64_t>(static_cast<double>(replay_offset.count()) / playback_speed_)),
            };

            const auto send_at{wall_origin_ + scaled_offset};

            const auto now{Clock::now()};

            if (send_at <= now)
            {
                return std::chrono::nanoseconds{0};
            }

            return std::chrono::nanoseconds(send_at - now);
        }
    };
}
