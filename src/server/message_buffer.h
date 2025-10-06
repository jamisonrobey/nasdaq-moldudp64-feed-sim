#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <optional>

class MessageBuffer
{
  public:
    explicit MessageBuffer(std::size_t buffer_size);

    void push(std::uint64_t seq, std::size_t pos);

    std::optional<std::size_t> seq_to_file_pos(std::uint64_t seq);

  private:
    struct Message
    {
        std::uint64_t seq_num;
        std::size_t file_pos;
    };

    std::vector<Message> buffer_;
    std::atomic<std::uint64_t> write_seq_{0};
};

#endif
