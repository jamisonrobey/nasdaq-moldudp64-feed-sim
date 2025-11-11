#ifndef RETRANSMISSION_BUFFER_H
#define RETRANSMISSION_BUFFER_H

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <optional>

/*
  container only thread safe if push() called from a single thread, which is the nature of MoldUDP64
  servers, we only have the one downstream feed/thread.
*/

class RetransmissionBuffer
{
  public:
    struct Message
    {
        std::uint64_t seq_num;
        std::size_t file_pos;
    };

    explicit RetransmissionBuffer(std::size_t buffer_size);

    void push(Message msg);

    [[nodiscard]]
    std::optional<std::size_t> get_file_pos_for_seq(std::uint64_t seq_num);

    [[nodiscard]]
    std::size_t size() const;

  private:
    std::vector<Message> buffer_;
    alignas(64) std::atomic<std::uint64_t> write_seq_{0};
};

#endif
