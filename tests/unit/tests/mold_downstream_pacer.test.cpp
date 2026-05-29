#include <chrono>
#include <gtest/gtest.h>

#include "imr/mold/downstream/pacer.h"

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
using namespace imr::mold::downstream;

class MoldDownstreamPacerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        MockClock::current_time = MockClock::time_point{ns(0)};
    }

    std::optional<ns> two_packet_delay(double speed, ns first_time, ns second_time, ns clock_at_second)
    {
        Pacer<MockClock> pacer({.playback_speed = speed});
        EXPECT_TRUE(pacer.get_delay(first_time).has_value());
        MockClock::current_time = MockClock::time_point{clock_at_second};
        return pacer.get_delay(second_time);
    }
};

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsNullopt_ForPacketBeforeIgnoreThreshold)
{
    const auto threshold{phase_to_ns(MarketPhase::open)};
    Pacer<MockClock> pacer({.skip_before = threshold});
    EXPECT_FALSE(pacer.get_delay(threshold - ns(1)).has_value());
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ProcessesPacket_AtExactIgnoreThreshold)
{
    const auto threshold{phase_to_ns(MarketPhase::open)};
    Pacer<MockClock> pacer({.skip_before = threshold});
    EXPECT_TRUE(pacer.get_delay(threshold).has_value());
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_ForFirstValidPacket)
{
    const auto threshold{phase_to_ns(MarketPhase::open)};
    Pacer<MockClock> pacer({.skip_before = threshold});
    const auto result{pacer.get_delay(threshold)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(0));
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_WhenScheduledTimeHasPassed)
{
    const auto result{two_packet_delay(1.0, ns(0), ns(500), ns(600))};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(0));
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_WhenScheduledTimeIsExactlyNow)
{
    const auto result{two_packet_delay(1.0, ns(0), ns(500), ns(500))};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(0));
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsPositiveDelay_WhenScheduledTimeIsInFuture)
{
    const auto result{two_packet_delay(1.0, ns(0), ns(1000), ns(200))};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(800));
}

TEST_F(MoldDownstreamPacerTest, GetDelay_HalfPlaybackSpeed_DoublesDelay)
{
    const auto result{two_packet_delay(0.5, ns(0), ns(1000), ns(200))};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(1800));
}

TEST_F(MoldDownstreamPacerTest, GetDelay_DoublePlaybackSpeed_HalvesDelay)
{
    const auto result{two_packet_delay(2.0, ns(0), ns(1000), ns(200))};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, ns(300));
}
