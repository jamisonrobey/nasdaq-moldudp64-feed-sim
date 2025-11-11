#ifndef RETRANSMISSION_SERVER
#define RETRANSMISSION_SERVER

#include "retransmission_buffer.h"

#include <thread>
#include <vector>
#include <sys/eventfd.h>

class RetransmissionServer
{
  public:
    RetransmissionServer(std::string_view session,
                         std::string_view address,
                         std::uint16_t port,
                         std::span<const std::byte> file,
                         RetransmissionBuffer* buffer,
                         std::size_t num_threads);

    void stop() const;

  private:
    int shutdown_fd_{eventfd(0, EFD_CLOEXEC)};
    std::vector<std::jthread> workers_;
};

#endif
