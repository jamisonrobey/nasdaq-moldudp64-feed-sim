#include "retransmission_server.h"
#include "retransmission_worker.h"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

RetransmissionServer::RetransmissionServer(std::string_view session,
                                           std::string_view address,
                                           std::uint16_t port,
                                           std::span<const std::byte> file,
                                           RetransmissionBuffer* retrans_buffer,
                                           std::size_t num_threads)
{
    for (std::size_t i = 0; i < num_threads; ++i)
    {
        workers_.emplace_back([session, address, port, file, retrans_buffer, this] {
            RetransmissionWorker worker{
                session,
                address,
                port,
                shutdown_fd_,
                file,
                retrans_buffer};
            worker.run();
        });
    }
}

void RetransmissionServer::stop() const
{
    constexpr std::uint64_t val{1};
    if (write(shutdown_fd_, &val, sizeof(val)) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }
}
