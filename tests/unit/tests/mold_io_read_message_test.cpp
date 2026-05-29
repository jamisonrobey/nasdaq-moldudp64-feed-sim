#include <gtest/gtest.h>

#include "mold/io.h"
#include "imr/mold/types.h"
#include "util/binary_io.h"

#include <array>

using namespace imr;

namespace
{
    template <std::size_t N>
    auto make_msg_block(std::array<char, N> body)
    {
        std::array<char, sizeof(mold::types::LengthPrefix) + N> block{};
        std::size_t pos{0};
        util::binary_io::write_be(std::span{block}, pos, static_cast<mold::types::LengthPrefix>(N));
        util::binary_io::write(std::span{block}, pos, std::span{body});
        return block;
    }
}

class MoldIoReadMessageTest : public ::testing::Test
{
  protected:
    std::size_t pos{0};

    void expect_success(std::span<const char> data, std::size_t expected_size)
    {
        const auto res{mold::io::read_message(data, pos)};
        EXPECT_EQ(res.size(), expected_size);
        EXPECT_EQ(pos, expected_size);
    }

    void expect_failure(std::span<const char> data)
    {
        const auto res{mold::io::read_message(data, pos)};
        EXPECT_TRUE(res.empty());
        EXPECT_EQ(pos, 0);
    }
};

TEST_F(MoldIoReadMessageTest, ValidMessage_ReturnsFullSpanAndAdvancesPos)
{
    constexpr std::array msg{'a', 'b', 'c'};
    const auto block{make_msg_block(msg)};
    expect_success(block, sizeof(mold::types::LengthPrefix) + msg.size());
}

TEST_F(MoldIoReadMessageTest, TruncatedLengthPrefix_ReturnsEmptyPosUnchanged)
{
    expect_failure(std::to_array<char>({0x00}));
}

TEST_F(MoldIoReadMessageTest, TruncatedBody_ReturnsEmptyPosUnchanged)
{
    constexpr std::array body{'a', 'b', 'c', 'd', 'e'};
    const auto block{make_msg_block(body)};
    expect_failure(std::span(block).subspan(0, sizeof(mold::types::LengthPrefix) + 2));
}
