#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <optional>

/*
  container only thread safe if push() called from a single thread, which is the nature of MoldUDP64
  servers, we only have the one downstream feed/thread.
*/

class MessageBuffer
{
  public:
    struct Message
    {
        std::uint64_t seq_num;
        std::size_t file_pos;
    };

    explicit MessageBuffer(std::size_t buffer_size);

    // todo: could be worth profiling to see if reference is faster but I think as this is only few
    // words should be faster to copy (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-in).
    void push(Message msg);

    [[nodiscard]]
    std::optional<std::size_t> get_file_pos_for_seq(std::uint64_t seq_num);

    [[nodiscard]]
    std::size_t size() const;

  private:
    std::vector<Message> buffer_;
    std::atomic<std::uint64_t> write_seq_{0};
};

#endif
