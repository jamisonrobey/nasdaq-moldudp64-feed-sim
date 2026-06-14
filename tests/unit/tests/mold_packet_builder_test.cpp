#include <gtest/gtest.h>

#include "imr/mold/packet_builder.h"
#include "imr/mold/types.h"
#include "util/binary_io.h"

#include <array>

using namespace imr;

class MoldPacketBuilderTest : public ::testing::Test
{
  protected:
    static constexpr mold::PacketBuilder::Config cfg{
        .session = "SESSION001",
        .MTU = 100,
    };
    static constexpr std::array<char, mold::PacketBuilder::min_message_size> min_msg{};

    mold::PacketBuilder builder{cfg};

    void add_messages(int n)
    {
        for (auto i{0}; i < n; ++i)
            ASSERT_TRUE(builder.try_add(min_msg));
    }
};

TEST_F(MoldPacketBuilderTest, Ctor_ValidConfig_CopiesSessionToHeader)
{
    auto packet{builder.finalize()};

    EXPECT_EQ(
        std::string_view(static_cast<const char*>(packet.front().iov_base), cfg.session.size()),
        cfg.session);
}

TEST_F(MoldPacketBuilderTest, Ctor_SessionLengthNot10_ThrowsInvalidArgument)
{
    EXPECT_THROW(mold::PacketBuilder packet_builder({.session = ""}), std::invalid_argument);
    // one under
    EXPECT_THROW(mold::PacketBuilder packet_builder({.session = "012345678"}), std::invalid_argument);
    // one over
    EXPECT_THROW(mold::PacketBuilder packet_builder({.session = "01234567890"}), std::invalid_argument);
}

TEST_F(MoldPacketBuilderTest, Ctor_SessionExactly10Chars_DoesNotThrow)
{
    EXPECT_NO_THROW(mold::PacketBuilder packet_builder({.session = "0123456789"}));
}

TEST_F(MoldPacketBuilderTest, Session_Always_ReturnsConfiguredSession)
{
    EXPECT_EQ(builder.session(), cfg.session);
}

TEST_F(MoldPacketBuilderTest, TryAdd_PacketFull_ReturnsFalse)
{
    constexpr auto capacity{(cfg.MTU - mold::types::header::length) / min_msg.size()};

    for (auto i{0UZ}; i < capacity; ++i)
        ASSERT_TRUE(builder.try_add(min_msg));

    EXPECT_FALSE(builder.try_add(min_msg));
}

TEST_F(MoldPacketBuilderTest, TryAdd_EmptyMessage_ReturnsFalse)
{
    EXPECT_FALSE(builder.try_add({}));
}

TEST_F(MoldPacketBuilderTest, TryAdd_MultipleMessages_AccumulatesMessageCount)
{
    constexpr auto to_add{5};
    add_messages(to_add);

    EXPECT_EQ(builder.message_count(), to_add);
}

TEST_F(MoldPacketBuilderTest, Finalize_AfterMessages_WritesMessageCountToHeader)
{
    constexpr auto to_add{5};
    add_messages(to_add);

    auto packet{builder.finalize()};

    const auto count{
        util::binary_io::read_at_be<mold::types::header::MessageCount>(
            std::span(static_cast<char*>(packet.front().iov_base), packet.front().iov_len),
            mold::types::header::message_count_offset),
    };

    EXPECT_EQ(count, to_add);
}

TEST_F(MoldPacketBuilderTest, Finalize_AfterMessages_ReturnsHeaderIovecPlusOnePerMessage)
{
    constexpr auto to_add{5};
    add_messages(to_add);

    auto packet{builder.finalize()};

    EXPECT_EQ(packet.size(), to_add + 1);
    EXPECT_EQ(packet[0].iov_len, mold::types::header::length);
    for (auto i{1UZ}; i < to_add; ++i)
    {
        EXPECT_EQ(packet[i].iov_len, min_msg.size());
    }
}

TEST_F(MoldPacketBuilderTest, Finalize_AfterMessage_MessageIovecPointsToOriginalData)
{
    std::array<char, mold::PacketBuilder::min_message_size> msg{'A', 'B', 'C'};
    ASSERT_TRUE(builder.try_add(msg));

    auto packet{builder.finalize()};

    EXPECT_EQ(packet[1].iov_base, msg.data());
}

TEST_F(MoldPacketBuilderTest, Reset_AfterMessages_ClearsMessageCount)
{
    add_messages(2);

    builder.reset(1);

    EXPECT_EQ(builder.message_count(), 0);
}

TEST_F(MoldPacketBuilderTest, Reset_WhenFull_AllowsRefill)
{
    while (builder.try_add(min_msg))
    {
    }

    builder.reset(1);

    EXPECT_TRUE(builder.try_add(min_msg));
}

TEST_F(MoldPacketBuilderTest, Reset_GivenSequenceNumber_WritesSequenceNumberToHeader)
{
    constexpr auto new_seq{99};

    builder.reset(new_seq);

    auto packet{builder.finalize()};

    const auto builder_seq{
        util::binary_io::read_at_be<mold::types::header::SequenceNumber>(
            std::span(static_cast<char*>(packet.front().iov_base), packet.front().iov_len),
            mold::types::header::sequence_number_offset),
    };

    EXPECT_EQ(builder_seq, new_seq);
}
