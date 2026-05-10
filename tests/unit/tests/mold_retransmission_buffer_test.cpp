#include <gtest/gtest.h>
#include "mold/retransmission_buffer.h"

namespace
{
    void push(mold::RetransmissionBuffer& buf, mold::SequenceNumber seq, std::size_t pos)
    {
        buf.push(mold::RetransmissionBuffer::MessageRecord{
            .sequence_number = seq,
            .file_position = pos,
        });
    }
}

TEST(RetransmissionBufferCtor, Zero_Throws)
{
    EXPECT_ANY_THROW(mold::RetransmissionBuffer(0));
}

TEST(RetransmissionBufferCtor, NonPowerOf2_Throws)
{
    EXPECT_ANY_THROW(mold::RetransmissionBuffer(3));
    EXPECT_ANY_THROW(mold::RetransmissionBuffer(6));
}

TEST(RetransmissionBufferCtor, PowerOf2_DoesNotThrow)
{
    EXPECT_NO_THROW(mold::RetransmissionBuffer(1));
    EXPECT_NO_THROW(mold::RetransmissionBuffer(2));
    EXPECT_NO_THROW(mold::RetransmissionBuffer(1 << 22));
}

TEST(RetransmissionBufferSize, ReturnsConstructorArg)
{
    mold::RetransmissionBuffer buffer(8);
    EXPECT_EQ(buffer.size(), 8u);
}

TEST(RetransmissionBufferPush, PushedRecord_IsRetrievable)
{
    mold::RetransmissionBuffer buffer(4);
    push(buffer, 1, 100);

    const auto result{buffer.file_position_for(1)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100u);
}

TEST(RetransmissionBufferPush, BeyondCapacity_OldestSlotOverwrittenNewestRetrievable)
{
    mold::RetransmissionBuffer buffer(4);
    push(buffer, 1, 100);
    push(buffer, 2, 200);
    push(buffer, 3, 300);
    push(buffer, 4, 400);
    push(buffer, 5, 500); // evicts seq=1

    EXPECT_EQ(buffer.file_position_for(1), std::nullopt);

    const auto result{buffer.file_position_for(5)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 500u);
}

TEST(RetransmissionBufferFilePositionFor, EmptyBuffer_ReturnsNullopt)
{
    mold::RetransmissionBuffer buffer(4);
    EXPECT_EQ(buffer.file_position_for(1), std::nullopt);
}

TEST(RetransmissionBufferFilePositionFor, FutureSequence_ReturnsNullopt)
{
    mold::RetransmissionBuffer buffer(4);
    push(buffer, 1, 100);
    EXPECT_EQ(buffer.file_position_for(2), std::nullopt);
}

TEST(RetransmissionBufferFilePositionFor, LiveWindowAfterWrap_ReturnsCorrectPositions)
{
    // buffer holds 4 entries; push 6 so it wraps — seq 1,2 evicted, seq 3..6 live
    mold::RetransmissionBuffer buffer(4);
    for (mold::SequenceNumber s = 1; s <= 6; ++s)
        push(buffer, s, s * 10);

    EXPECT_EQ(buffer.file_position_for(1), std::nullopt);
    EXPECT_EQ(buffer.file_position_for(2), std::nullopt);

    for (mold::SequenceNumber s = 3; s <= 6; ++s)
    {
        const auto result{buffer.file_position_for(s)};
        ASSERT_TRUE(result.has_value()) << "expected live entry for seq=" << s;
        EXPECT_EQ(*result, s * 10);
    }
}
