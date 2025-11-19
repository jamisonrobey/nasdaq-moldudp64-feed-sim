#include "temporal_pacer.h"
#include <chrono>
#include <gtest/gtest.h>

struct MockClock
{
    using time_point = std::chrono::steady_clock::time_point;

    static time_point current_time;

    static time_point now()
    {
        return current_time;
    }

    static void advance(std::chrono::nanoseconds duration) { current_time += duration; }
};

MockClock::time_point MockClock::current_time{};

using namespace std::chrono_literals;

class PacerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        MockClock::current_time = MockClock::time_point{0ns};
    }

    const std::chrono::nanoseconds start_after_100ms{100ms};
    const MockClock::time_point wall_epoch{MockClock::time_point{0ns}};
};

TEST_F(PacerTest, GetDelay_ReturnsNullopt_ForTimestampsBeforeStartAfter)
{
    constexpr auto replay_speed{1.0};
    TemporalPacer<MockClock> pacer{replay_speed, start_after_100ms, wall_epoch};

    const auto delay{pacer.get_delay(50ms)};

    EXPECT_FALSE(delay.has_value());
}

TEST_F(PacerTest, GetDelay_EstablishesEpoch_AndReturnsZero)
{
    constexpr auto replay_speed{1.0};
    TemporalPacer<MockClock> pacer{replay_speed, start_after_100ms, wall_epoch};

    const auto delay{pacer.get_delay(150ms)};

    EXPECT_TRUE(delay.has_value());
    EXPECT_EQ(delay.value(), 0ns);
}

TEST_F(PacerTest, GetDelay_CalculatesCorrectDelay_OnNormalReplay)
{
    constexpr double replay_speed{1.0};
    TemporalPacer<MockClock> pacer{replay_speed, start_after_100ms, wall_epoch};

    constexpr auto first_timestamp{150ms};
    constexpr auto second_timestamp{first_timestamp + 200ms};

    pacer.get_delay(first_timestamp);

    // Simulate some minor work happening between packets
    MockClock::advance(5ms);

    const auto delay = pacer.get_delay(second_timestamp);

    // elapsed 200ms between timestamp
    // scaled time:  200ms / 1.0 = 200ms
    // target send: wall_epoch (0ns) + 200ms = 200ms
    // result: target - now(5ms mocked) = 195ms
    EXPECT_TRUE(delay.has_value());
    EXPECT_EQ(delay.value(), 195ms);
}

TEST_F(PacerTest, GetDelay_CalculatesShorterDelay_OnFastReplay)
{
    constexpr double replay_speed{2.0};
    TemporalPacer<MockClock> pacer{replay_speed, start_after_100ms, wall_epoch};

    constexpr auto first_timestamp{150ms};
    constexpr auto second_timestamp{first_timestamp + 200ms};

    pacer.get_delay(first_timestamp);

    MockClock::advance(5ms);

    const auto delay = pacer.get_delay(second_timestamp);

    // elapsed 200ms between timestamp
    // scaled time:  200ms /  replay_speed = 100ms
    // target send: wall_epoch (0ns) + 100ms = 100ms
    // result: target - now(5ms mocked) = 95ms
    EXPECT_TRUE(delay.has_value());
    EXPECT_EQ(delay.value(), 95ms);
}

TEST_F(PacerTest, GetDelay_ReturnsZero_WhenReplayIsLagging)
{
    constexpr double replay_speed{1.0};
    TemporalPacer<MockClock> pacer{replay_speed, start_after_100ms, wall_epoch};

    constexpr auto first_timestamp{150ms};
    constexpr auto second_timestamp{first_timestamp + 50ms};

    pacer.get_delay(first_timestamp);

    MockClock::advance(100ms);

    const auto delay = pacer.get_delay(second_timestamp);

    // we are behind by 50 ms so we should get no delay
    EXPECT_TRUE(delay.has_value());
    EXPECT_EQ(delay.value(), 0ns);
}
