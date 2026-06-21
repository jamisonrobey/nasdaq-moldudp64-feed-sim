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

using namespace std::chrono_literals;
using namespace imr::mold::downstream;

class MoldDownstreamPacerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        MockClock::current_time = MockClock::time_point{0ns};
    }

    std::optional<std::chrono::nanoseconds> two_packet_delay(double speed, std::chrono::nanoseconds first_time, std::chrono::nanoseconds second_time, std::chrono::nanoseconds clock_at_second)
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
    using namespace std::chrono_literals;
    EXPECT_FALSE(pacer.get_delay(threshold - 1ns).has_value());
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
    EXPECT_EQ(*result, 0ns);
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_WhenScheduledTimeHasPassed)
{
    const auto result{two_packet_delay(1.0, 0ns, 500ns, 600ns)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0ns);
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_WhenScheduledTimeIsExactlyNow)
{
    const auto result{two_packet_delay(1.0, 0ns, 500ns, 500ns)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0ns);
}

TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsPositiveDelay_WhenScheduledTimeIsInFuture)
{
    const auto result{two_packet_delay(1.0, 0ns, 1000ns, 200ns)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 800ns);
}

TEST_F(MoldDownstreamPacerTest, GetDelay_HalfPlaybackSpeed_DoublesDelay)
{
    using namespace std::chrono_literals;

    const auto result{two_packet_delay(0.5, 0ns, 1000ns, 200ns)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1800ns);
}

TEST_F(MoldDownstreamPacerTest, GetDelay_DoublePlaybackSpeed_HalvesDelay)
{
    const auto result{two_packet_delay(2.0, 0ns, 1000ns, 200ns)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 300ns);
}
