#include <gtest/gtest.h>
#include <limits>

#include "retransmission_buffer.h"

namespace
{

void expect_found(RetransmissionBuffer& buff, std::uint64_t seq_num, std::size_t expected_pos)
{
    const auto res{buff.get_file_pos_for_seq(seq_num)};
    ASSERT_TRUE(res.has_value())
        << "Expected seq_num " << seq_num << " to be found in buffer";
    EXPECT_EQ(*res, expected_pos)
        << "seq_num " << seq_num << " has wrong file_pos";
}

void expect_not_found(RetransmissionBuffer& buff, std::uint64_t seq_num)
{
    const auto res{buff.get_file_pos_for_seq(seq_num)};
    EXPECT_FALSE(res.has_value())
        << "Expected seq_num " << seq_num << " to NOT be found in buffer";
}

void push_range(RetransmissionBuffer& buff, std::uint64_t start_seq, std::uint64_t count)
{
    for (std::uint64_t i = 0; i < count; ++i)
    {
        const std::uint64_t seq{start_seq + i};
        buff.push({.seq_num = seq, .file_pos = seq});
    }
}

}

TEST(RetransmissionBufferTest, Constructor_ValidatesPowerOfTwo)
{
    EXPECT_NO_THROW(RetransmissionBuffer(4));
    EXPECT_NO_THROW(RetransmissionBuffer(1024));

    EXPECT_THROW(RetransmissionBuffer(0), std::invalid_argument);
    EXPECT_THROW(RetransmissionBuffer(3), std::invalid_argument);
    EXPECT_THROW(RetransmissionBuffer(5), std::invalid_argument);
}

TEST(RetransmissionBufferTest, EmptyBuffer_ReturnsNullopt)
{
    RetransmissionBuffer buff{4};
    expect_not_found(buff, 0); // seq_num 0 is invalid; but guards against matching default-initialized buffer slots
    expect_not_found(buff, 1);
    expect_not_found(buff, std::numeric_limits<std::uint64_t>::max());
}

TEST(RetransmissionBufferTest, PushAndRetrieve_Works)
{
    RetransmissionBuffer buff{2};

    buff.push({.seq_num = 1, .file_pos = 1000});
    buff.push({.seq_num = 2, .file_pos = 2000});

    expect_found(buff, 1, 1000);
    expect_found(buff, 2, 2000);
}

TEST(RetransmissionBufferTest, WrapAround_EvictsOldest)
{
    RetransmissionBuffer buff{4};

    push_range(buff, 1, 5); // write over oldest

    expect_not_found(buff, 1); // out of range
    expect_found(buff, 2, 2);  // new oldest
    expect_found(buff, 5, 5);  // latest
}

TEST(RetransmissionBufferTest, Boundaries_TooOldAndTooNew)
{
    RetransmissionBuffer buff{4};

    push_range(buff, 1, 8); // contains 5,6,7,8

    // out of range
    expect_not_found(buff, 4);
    expect_not_found(buff, 9);

    expect_found(buff, 5, 5); // oldest
    expect_found(buff, 8, 8); // latest
}

TEST(RetransmissionBufferTest, SlotCollision_OverwritesCorrectly)
{
    RetransmissionBuffer buff{4};

    push_range(buff, 1, 4);
    buff.push({.seq_num = 5, .file_pos = 5000});

    expect_not_found(buff, 1);
    expect_found(buff, 5, 5000);
}
