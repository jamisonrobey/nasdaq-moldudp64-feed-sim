#include "temporal_pacer.h"
#include <chrono>
#include <print>

TemporalPacer::TemporalPacer(double time_scale, std::chrono::nanoseconds start_after)
    : time_scale_{time_scale},
      start_after_{start_after},
      wall_epoch_{std::chrono::steady_clock::now()},
      message_epoch_{0}
{
}
std::optional<std::chrono::nanoseconds> TemporalPacer::get_delay(std::chrono::nanoseconds packet_timestamp)
{
    if (packet_timestamp < start_after_)
    {
        return std::nullopt;
    }
    if (message_epoch_.count() == 0)
    {
        message_epoch_ = packet_timestamp;
        wall_epoch_ = std::chrono::steady_clock::now();
        // std::println("INIT: message_epoch={}, wall_epoch={}",
        //              message_epoch_.count(),
        //              wall_epoch_.time_since_epoch().count());
        return std::chrono::nanoseconds{0};
    }
    const auto message_elapsed{packet_timestamp - message_epoch_};
    const auto scaled_elapsed{std::chrono::nanoseconds(
        static_cast<int64_t>(static_cast<double>(message_elapsed.count()) / time_scale_))};
    const auto target_time{wall_epoch_ + scaled_elapsed};
    const auto now = std::chrono::steady_clock::now();

    // std::println("packet_ts={}, msg_elapsed={}, scaled={}, target={}, now={}, diff={}",
    //              packet_timestamp.count(),
    //              message_elapsed.count(),
    //              scaled_elapsed.count(),
    //              target_time.time_since_epoch().count(),
    //              now.time_since_epoch().count(),
    //              (target_time - now).count());

    if (target_time <= now)
    {
        return std::chrono::nanoseconds{0};
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(target_time - now);
}
