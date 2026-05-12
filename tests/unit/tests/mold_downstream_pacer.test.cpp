#include <chrono>
#include <gtest/gtest.h>

#include "mold/downstream/pacer.h"

namespace
{
    struct MockClock
    {
        using duration = std::chrono::nanoseconds;
        using time_point = std::chrono::time_point<MockClock, duration>;

        static time_point current_time;
        static time_point now() { return current_time; }
        static void advance(std::chrono::nanoseconds d) { current_time += d; }
    };
    MockClock::time_point MockClock::current_time{};
}

using namespace std::chrono_literals;
using ns = std::chrono::nanoseconds;
using namespace mold::downstream;

TEST(MoldDownstreamPacer, ReturnsNullopt_ForPacketBeforeIgnoreThreshold)
{
    constexpr auto phase{MarketPhase::open};
    const auto delay{Pacer<MockClock>::phase_timestamp(phase)};
    Pacer<MockClock> pacer({
        .ignore_packets_before_phase = phase,
    });

    EXPECT_FALSE(pacer.get_delay(delay - ns(1)).has_value());
}

TEST(MoldDownstreamPacer, ProcessesPacket_AtExactIgnoreThreshold)
{
    constexpr auto phase{MarketPhase::open};
    const auto delay{Pacer<MockClock>::phase_timestamp(phase)};
    Pacer<MockClock> pacer({
        .ignore_packets_before_phase = phase,
    });

    EXPECT_TRUE(pacer.get_delay(delay).has_value());
}

TEST(MoldDownstreamPacer, ReturnsZeroDelay_ForFirstValidPacket)
{
    constexpr auto phase{MarketPhase::open};
    const auto ignore_threshold{Pacer<MockClock>::phase_timestamp(phase)};
    Pacer<MockClock> pacer({
        .ignore_packets_before_phase = phase,
    });

    const auto result{pacer.get_delay(ignore_threshold)};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(0));
}

TEST(MoldDownstreamPacer, ReturnsZeroDelay_WhenScheduledTimeHasPassed)
{
    constexpr auto first_packet_time{0};
    constexpr auto second_packet_time{500};
    constexpr auto current_time{600};
    MockClock::current_time = MockClock::time_point{ns{current_time}};
    Pacer<MockClock> pacer({
        .playback_wall_start = MockClock::time_point{ns{0}},
    });
    ASSERT_TRUE(pacer.get_delay(ns(first_packet_time)).has_value());

    const auto result{pacer.get_delay(ns(second_packet_time))};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(0));
}

TEST(MoldDownstreamPacer, ReturnsZeroDelay_WhenScheduledTimeIsExactlyNow)
{
    constexpr auto first_packet_time{0};
    constexpr auto second_packet_time{500};
    constexpr auto current_time{second_packet_time};
    MockClock::current_time = MockClock::time_point{ns{current_time}};
    Pacer<MockClock> pacer({
        .playback_wall_start = MockClock::time_point{ns{0}},
    });
    ASSERT_TRUE(pacer.get_delay(ns(first_packet_time)).has_value());

    const auto result{pacer.get_delay(ns(second_packet_time))};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(0));
}

TEST(MoldDownstreamPacer, ReturnsPositiveDelay_WhenScheduledTimeIsInFuture)
{
    constexpr auto speed{1.0};
    constexpr auto first_packet_time{0};
    constexpr auto second_packet_time{1000};
    constexpr auto current_time{200};
    constexpr auto expected_delay{second_packet_time - current_time};
    MockClock::current_time = MockClock::time_point{ns{current_time}};
    Pacer<MockClock> pacer({
        .playback_speed = speed,
        .playback_wall_start = MockClock::time_point{ns{0}},
    });
    ASSERT_TRUE(pacer.get_delay(ns(first_packet_time)).has_value());

    const auto result{pacer.get_delay(ns(second_packet_time))};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(expected_delay));
}

TEST(MoldDownstreamPacer, HalfPlaybackSpeed_DoublesDelay)
{
    constexpr auto speed{0.5};
    constexpr auto first_packet_time{0};
    constexpr auto second_packet_time{1000};
    constexpr auto current_time{200};
    constexpr auto expected_delay{static_cast<int>(second_packet_time / speed) - current_time};
    MockClock::current_time = MockClock::time_point{ns{current_time}};
    Pacer<MockClock> pacer({
        .playback_speed = speed,
        .playback_wall_start = MockClock::time_point{ns{0}},
    });
    ASSERT_TRUE(pacer.get_delay(ns(first_packet_time)).has_value());

    const auto result{pacer.get_delay(ns(second_packet_time))};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(expected_delay));
}

TEST(MoldDownstreamPacer, DoublePlaybackSpeed_HalvesDelay)
{
    constexpr auto speed{2.0};
    constexpr auto first_packet_time{0};
    constexpr auto second_packet_time{1000};
    constexpr auto current_time{200};
    constexpr auto expected_delay{static_cast<int>(second_packet_time / speed) - current_time};
    MockClock::current_time = MockClock::time_point{ns{current_time}};
    Pacer<MockClock> pacer({
        .playback_speed = speed,
        .playback_wall_start = MockClock::time_point{ns{0}},
    });
    ASSERT_TRUE(pacer.get_delay(ns(first_packet_time)).has_value());

    const auto result{pacer.get_delay(ns(second_packet_time))};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(expected_delay));
}
