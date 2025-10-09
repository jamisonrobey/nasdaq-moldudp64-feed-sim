#include <gtest/gtest.h>
#include <limits>

#include "server/message_buffer.h"

class MessageBufferTest : public ::testing::Test
{
  protected:
    static void expect_found(MessageBuffer& buff, std::uint64_t seq_num, std::size_t expected_pos)
    {
        const auto res{buff.get_file_pos_for_seq(seq_num)};
        ASSERT_TRUE(res.has_value())
            << "Expected seq_num " << seq_num << " to be found in buffer";
        EXPECT_EQ(*res, expected_pos)
            << "seq_num " << seq_num << " has wrong file_pos";
    }

    static void expect_not_found(MessageBuffer& buff, std::uint64_t seq_num)
    {
        const auto res{buff.get_file_pos_for_seq(seq_num)};
        EXPECT_FALSE(res.has_value())
            << "Expected seq_num " << seq_num << " to NOT be found in buffer";
    }

    static void push_range(MessageBuffer& buff, std::uint64_t start_seq, std::uint64_t count)
    {
        for (std::uint64_t i = 0; i < count; ++i)
        {
            const std::uint64_t seq{start_seq + i};
            buff.push({.seq_num = seq, .file_pos = seq});
        }
    }
};

TEST_F(MessageBufferTest, Constructor_ValidatesPowerOfTwo)
{
    EXPECT_NO_THROW(MessageBuffer(4));
    EXPECT_NO_THROW(MessageBuffer(1024));

    EXPECT_THROW(MessageBuffer(0), std::invalid_argument);
    EXPECT_THROW(MessageBuffer(3), std::invalid_argument);
    EXPECT_THROW(MessageBuffer(5), std::invalid_argument);
}

TEST_F(MessageBufferTest, EmptyBuffer_ReturnsNullopt)
{
    MessageBuffer buff{4};
    expect_not_found(buff, 0); // seq_num 0 is invalid; but guards against matching default-initialized buffer slots
    expect_not_found(buff, 1);
    expect_not_found(buff, std::numeric_limits<std::uint64_t>::max());
}

TEST_F(MessageBufferTest, PushAndRetrieve_Works)
{
    MessageBuffer buff{2};

    buff.push({.seq_num = 1, .file_pos = 1000});
    buff.push({.seq_num = 2, .file_pos = 2000});

    expect_found(buff, 1, 1000);
    expect_found(buff, 2, 2000);
}

TEST_F(MessageBufferTest, WrapAround_EvictsOldest)
{
    MessageBuffer buff{4};

    push_range(buff, 1, 5); // write over oldest

    expect_not_found(buff, 1); // out of range
    expect_found(buff, 2, 2);  // new oldest
    expect_found(buff, 5, 5);  // latest
}

TEST_F(MessageBufferTest, Boundaries_TooOldAndTooNew)
{
    MessageBuffer buff{4};

    push_range(buff, 1, 8); // contains 5,6,7,8

    // out of range
    expect_not_found(buff, 4);
    expect_not_found(buff, 9);

    expect_found(buff, 5, 5); // oldest
    expect_found(buff, 8, 8); // latest
}

TEST_F(MessageBufferTest, SlotCollision_OverwritesCorrectly)
{
    MessageBuffer buff{4};

    push_range(buff, 1, 4);
    buff.push({.seq_num = 5, .file_pos = 5000});

    expect_not_found(buff, 1);
    expect_found(buff, 5, 5000);
}