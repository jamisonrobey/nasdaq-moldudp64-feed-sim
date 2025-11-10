#include "mold_udp_64.h"
#include "packet_builder.h"
#include <gtest/gtest.h>
#include <arpa/inet.h>

namespace
{

constexpr std::string_view session{"SESSION001"};

void test_default_state(PacketBuilder& builder)
{
    EXPECT_EQ(builder.size(), sizeof(MoldUDP64::DownstreamHeader));
    EXPECT_TRUE(builder.empty());
    EXPECT_EQ(builder.msg_count(), 0);
}

}

TEST(PacketBuilderTest, Constructor_InitializesToEmptyState)
{
    PacketBuilder builder{session};
    std::string_view builder_session{builder.session()};

    test_default_state(builder);
    EXPECT_EQ(session, builder_session);
}

TEST(PacketBuilderTest, Reset_ClearsMessagesAndSetsSequenceNumber)
{
    PacketBuilder builder{session};
    std::array<std::byte, 1> msg{};
    constexpr auto new_seq_num{123};

    builder.try_add_message(msg);
    builder.reset(new_seq_num);

    test_default_state(builder);
    EXPECT_EQ(new_seq_num, builder.seq_num());
}

TEST(PacketBuilderTest, TryAddMessage_SucceedsWhenSpaceIsAvailable)
{
    PacketBuilder builder{session};
    std::array<std::byte, 1> msg{};

    EXPECT_TRUE(builder.try_add_message(msg));
}

TEST(PacketBuilderTest, TryAddMessage_FailsWhenMessageTooLarge)
{
    PacketBuilder builder{session};
    std::array<std::byte, MoldUDP64::max_payload_size> msg{};

    EXPECT_FALSE(builder.try_add_message(msg));
}

TEST(PacketBuilderTest, TryAddMessage_SucceedsOnExactFit)
{
    PacketBuilder builder{session};
    std::array<std::byte, MoldUDP64::max_message_block_size> msg{};

    EXPECT_TRUE(builder.try_add_message(msg));
}

TEST(PacketBuilderTest, Finalize_WritesCorrectHeaderWithNetworkOrder)
{
    PacketBuilder builder{session};
    std::array<std::byte, 1> msg{};
    constexpr std::uint64_t seq_num{1};
    MoldUDP64::DownstreamHeader builder_header{};

    builder.reset(seq_num);
    builder.try_add_message(msg);
    builder.finalize();
    std::memcpy(&builder_header, builder.cbegin(), sizeof(builder_header));

    std::string_view builder_session{builder_header.session};
    EXPECT_EQ(session, builder_session);
    EXPECT_EQ(htons(1), builder.msg_count());
    EXPECT_EQ(htobe64(seq_num), builder_header.sequence_num);
}

TEST(PacketBuilderTest, MessageBlock_ContainsCorrectData)
{
    auto create_sequenced_message = []<std::size_t N>() {
        std::array<std::byte, N> message{};
        for (std::size_t i = 0; i < N; ++i)
        {
            message[i] = static_cast<std::byte>(i);
        }
        return message;
    };

    PacketBuilder builder{session};
    const auto msg1 = create_sequenced_message.operator()<10>();
    const auto msg2 = create_sequenced_message.operator()<20>();

    builder.try_add_message(msg1);
    builder.try_add_message(msg2);

    const auto message_block = builder.message_block();

    ASSERT_EQ(builder.size(), sizeof(MoldUDP64::DownstreamHeader) + msg1.size() + msg2.size());

    EXPECT_TRUE(std::ranges::equal(message_block.subspan(0, msg1.size()), msg1));

    EXPECT_TRUE(std::ranges::equal(message_block.subspan(msg1.size()), msg2));
}
