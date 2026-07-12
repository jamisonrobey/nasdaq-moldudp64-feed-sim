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
using namespace imr::mold::downstream;

class MoldDownstreamPacerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        MockClock::current_time = MockClock::time_point{0ns};
    }
    std::chrono::nanoseconds two_packet_delay(double speed,
                                              std::chrono::nanoseconds first_time,
                                              std::chrono::nanoseconds second_time,
                                              std::chrono::nanoseconds clock_at_second)
    {
        Pacer<MockClock> pacer({.playback_speed = speed});
        // store in variable because of [[nodiscard]]
        const auto first{pacer.get_delay(first_time)};
        MockClock::current_time = MockClock::time_point{clock_at_second};
        return pacer.get_delay(second_time);
    }
};

TEST_F(MoldDownstreamPacerTest, ShouldSkip_ReturnsTrue_ForPacketBeforeIgnoreThreshold)
{
    const auto threshold{phase_to_ns(MarketPhase::open)};
    Pacer<MockClock> pacer({.skip_before = threshold});

    EXPECT_TRUE(pacer.should_skip(threshold - 1ns));
}
TEST_F(MoldDownstreamPacerTest, ShouldSkip_ReturnsFalse_AtExactIgnoreThreshold)
{
    const auto threshold{phase_to_ns(MarketPhase::open)};
    Pacer<MockClock> pacer({.skip_before = threshold});

    EXPECT_FALSE(pacer.should_skip(threshold));
}
TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_ForFirstValidPacket)
{
    const auto threshold{phase_to_ns(MarketPhase::open)};
    Pacer<MockClock> pacer({.skip_before = threshold});

    const auto result{pacer.get_delay(threshold)};

    EXPECT_EQ(result, 0ns);
}
TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_WhenScheduledTimeHasPassed)
{
    const auto result{two_packet_delay(1.0, 0ns, 500ns, 600ns)};

    EXPECT_EQ(result, 0ns);
}
TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsZeroDelay_WhenScheduledTimeIsExactlyNow)
{
    const auto result{two_packet_delay(1.0, 0ns, 500ns, 500ns)};

    EXPECT_EQ(result, 0ns);
}
TEST_F(MoldDownstreamPacerTest, GetDelay_ReturnsPositiveDelay_WhenScheduledTimeIsInFuture)
{
    const auto result{two_packet_delay(1.0, 0ns, 1000ns, 200ns)};

    EXPECT_EQ(result, 800ns);
}
TEST_F(MoldDownstreamPacerTest, GetDelay_HalfPlaybackSpeed_DoublesDelay)
{
    const auto result{two_packet_delay(0.5, 0ns, 1000ns, 200ns)};

    EXPECT_EQ(result, 1800ns);
}
TEST_F(MoldDownstreamPacerTest, GetDelay_DoublePlaybackSpeed_HalvesDelay)
{
    const auto result{two_packet_delay(2.0, 0ns, 1000ns, 200ns)};

    EXPECT_EQ(result, 300ns);
}
