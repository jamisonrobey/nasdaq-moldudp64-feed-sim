#include <gtest/gtest.h>

#include "mold/io.h"
#include "imr/mold/types.h"
#include "util/binary_io.h"

#include <array>

using namespace imr;

namespace
{
    // builds a MoldUDP64 message block: [uint16_be length | body]
    template <std::size_t N>
    auto make_msg_block(std::array<char, N> body)
    {
        std::array<char, sizeof(imr::mold::types::LengthPrefix) + N> block{};
        std::size_t pos = 0;
        util::binary_io::write_be(std::span{block}, pos, static_cast<imr::mold::types::LengthPrefix>(N));
        util::binary_io::write(std::span{block}, pos, std::span{body});
        return block;
    }
}

TEST(MoldIoReadMessage, ValidMessage_ReturnsFullSpanAndAdvancesPos)
{
    constexpr std::array msg{'a', 'b', 'c'};
    const auto block{make_msg_block(msg)};
    std::size_t pos{0};

    const auto res{mold::io::read_message(std::span<const char>(block), pos)};

    EXPECT_EQ(res.size(), sizeof(imr::mold::types::LengthPrefix) + msg.size());
    EXPECT_EQ(pos, sizeof(imr::mold::types::LengthPrefix) + msg.size());
}

TEST(MoldIoReadMessage, TruncatedLengthPrefix_ReturnsEmptyPosUnchanged)
{
    // len prefix must be at least 2-byte so give truncated 1-byte prefix
    constexpr auto bytes{std::to_array<char>({0x00})};
    std::size_t pos{0};

    const auto res{mold::io::read_message(std::span(bytes), pos)};

    EXPECT_TRUE(res.empty());
    EXPECT_EQ(pos, 0);
}

TEST(MoldIoReadMessage, TruncatedBody_ReturnsEmptyPosUnchanged)
{
    constexpr std::array body{'a', 'b', 'c', 'd', 'e'};
    const auto block{make_msg_block(body)};
    std::size_t pos{0};

    // truncate block so length prefix claims more bytes than it actually contains
    const auto truncated{std::span(block).subspan(0, sizeof(imr::mold::types::LengthPrefix) + 2)};
    const auto res{mold::io::read_message(truncated, pos)};

    EXPECT_TRUE(res.empty());
    EXPECT_EQ(pos, 0);
}
