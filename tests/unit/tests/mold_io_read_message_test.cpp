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
    }
};

TEST_F(MoldIoReadMessageTest, ValidMessage_ReturnsFullSpanAndAdvancesPos)
{
    constexpr std::array msg{'a', 'b', 'c'};
    const auto block{make_msg_block(msg)};

    const auto res{mold::io::read_message(block, pos)};

    constexpr auto expected_size{sizeof(mold::types::LengthPrefix) + msg.size()};

    EXPECT_EQ(res.size(), expected_size);
    EXPECT_EQ(pos, expected_size);
}

TEST_F(MoldIoReadMessageTest, TruncatedLengthPrefix_ReturnsEmpty)
{

    const auto res{mold::io::read_message(std::to_array<char>({0x00}), pos)};

    EXPECT_TRUE(res.empty());
}

TEST_F(MoldIoReadMessageTest, TruncatedBody_ReturnsEmpty)
{
    constexpr std::array body{'a', 'b', 'c', 'd', 'e'};
    const auto block{make_msg_block(body)};

    // give block missing last byte
    const auto res{mold::io::read_message(std::span(block.data(), block.size() - 2), pos)};

    EXPECT_TRUE(res.empty());
}
