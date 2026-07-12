#ifndef TEMPORAL_PACER_H
#define TEMPORAL_PACER_H
#include <chrono>
#include <concepts>

template <typename T>
concept ClockConcept =
    requires {
        typename T::time_point;
        { T::now() } -> std::same_as<typename T::time_point>;
    };

template <ClockConcept Clock = std::chrono::steady_clock>
class TemporalPacer
{
  public:
    TemporalPacer(double time_scale, std::chrono::nanoseconds start_after, Clock::time_point wall_epoch)
        : time_scale_{time_scale},
          start_after_{start_after},
          wall_epoch_{wall_epoch},
          message_epoch_{0}
    {
    }

    std::optional<std::chrono::nanoseconds> get_delay(std::chrono::nanoseconds packet_timestamp)
    {
        if (packet_timestamp < start_after_)
        {
            return std::nullopt;
        }
        if (message_epoch_.count() == 0)
        {
            message_epoch_ = packet_timestamp;
            return std::chrono::nanoseconds{0};
        }

        const auto message_elapsed{packet_timestamp - message_epoch_};
        const auto scaled_elapsed{std::chrono::nanoseconds(
            static_cast<int64_t>(static_cast<double>(message_elapsed.count()) / time_scale_))};
        const auto target_time{wall_epoch_ + scaled_elapsed};
        const auto now = Clock::now();

        if (target_time <= now)
        {
            return std::chrono::nanoseconds{0};
        }
        return std::chrono::duration_cast<std::chrono::nanoseconds>(target_time - now);
    }

  private:
    double time_scale_;
    std::chrono::nanoseconds start_after_;
    std::chrono::steady_clock::time_point wall_epoch_;
    std::chrono::nanoseconds message_epoch_;
};

#endif
