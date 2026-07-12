#include <gtest/gtest.h>

#include "imr/mold/retransmission_buffer.h"

using namespace imr;

class RetransmissionBufferTest : public ::testing::Test
{
  protected:
    mold::RetransmissionBuffer buf{4};

    void push(mold::types::header::SequenceNumber seq, std::size_t pos)
    {
        buf.push({.sequence_number = seq, .file_position = pos});
    }
};

TEST_F(RetransmissionBufferTest, Ctor_Zero_Throws)
{
    EXPECT_THROW(mold::RetransmissionBuffer(0), std::invalid_argument);
}

TEST_F(RetransmissionBufferTest, Size_ReturnsConstructorArg)
{
    EXPECT_EQ(buf.size(), 4u);
}

TEST_F(RetransmissionBufferTest, Push_Record_IsRetrievable)
{
    push(1, 100);
    const auto result{buf.file_position_for(1)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100u);
}

TEST_F(RetransmissionBufferTest, Push_BeyondCapacity_OldestEvictedNewestRetrievable)
{
    push(1, 100);
    push(2, 200);
    push(3, 300);
    push(4, 400);
    push(5, 500); // evicts seq=1
    EXPECT_EQ(buf.file_position_for(1), std::nullopt);
    const auto result{buf.file_position_for(5)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 500u);
}

TEST_F(RetransmissionBufferTest, FilePositionFor_EmptyBuffer_ReturnsNullopt)
{
    EXPECT_EQ(buf.file_position_for(1), std::nullopt);
}

TEST_F(RetransmissionBufferTest, FilePositionFor_FutureSequence_ReturnsNullopt)
{
    push(1, 100);
    EXPECT_EQ(buf.file_position_for(2), std::nullopt);
}

TEST_F(RetransmissionBufferTest, FilePositionFor_LiveWindowAfterWrap_ReturnsCorrectPositions)
{
    // push 6 entries into size-4 buffer; seq 1,2 evicted, seq 3..6 live
    for (mold::types::header::SequenceNumber seq = 1; seq <= 6; ++seq)
    {
        push(seq, seq * 10);
    }

    EXPECT_EQ(buf.file_position_for(1), std::nullopt);
    EXPECT_EQ(buf.file_position_for(2), std::nullopt);
    for (mold::types::header::SequenceNumber seq = 3; seq <= 6; ++seq)
    {
        const auto result{buf.file_position_for(seq)};
        ASSERT_TRUE(result.has_value()) << "expected live entry for seq=" << seq;
        EXPECT_EQ(*result, seq * 10);
    }
}
