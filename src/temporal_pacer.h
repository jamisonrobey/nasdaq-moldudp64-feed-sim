#ifndef TEMPORAL_PACER_H
#define TEMPORAL_PACER_H

#include <chrono>
class TemporalPacer
{
  public:
    TemporalPacer(double time_scale, std::chrono::nanoseconds start_after);

    std::optional<std::chrono::nanoseconds> get_delay(std::chrono::nanoseconds packet_timestamp);

  private:
    double time_scale_;
    std::chrono::nanoseconds start_after_;
    std::chrono::steady_clock::time_point wall_epoch_;
    std::chrono::nanoseconds message_epoch_;
};

#endif
