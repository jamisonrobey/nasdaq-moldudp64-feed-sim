#include "retransmission_worker.h"
#include "itch.h"
#include "mold_udp_64.h"

#include <arpa/inet.h>
#include <endian.h>
#include <sys/epoll.h>
#include <print>
#include <iostream>

RetransmissionWorker::RetransmissionWorker(std::string_view session,
                                           std::string_view address,
                                           std::uint16_t port,
                                           int shutdown_fd,
                                           std::span<const std::byte> file,
                                           MessageBuffer* msg_buffer)
    : packet_builder_{session},
      shutdown_fd_{shutdown_fd},
      file_{file},
      msg_buffer_{msg_buffer},
      sock_{socket(AF_INET, SOCK_DGRAM, 0)},
      epfd_{epoll_create1(0)}
{
    constexpr auto sockopt_on{1};
    if (setsockopt(sock_.fd(), SOL_SOCKET, SO_REUSEPORT, &sockopt_on, sizeof(sockopt_on)) < 0 ||
        setsockopt(sock_.fd(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (const auto ret{inet_pton(AF_INET,
                                 address.data(),
                                 &addr.sin_addr)};
        ret == 0)
    {
        throw std::invalid_argument(std::format("invalid ip format for request address {}", address));
    }
    else if (ret < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    if (bind(sock_.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = sock_.fd();

    if (epoll_ctl(epfd_.fd(), EPOLL_CTL_ADD, sock_.fd(), &event) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    event.data.fd = shutdown_fd_;

    if (epoll_ctl(epfd_.fd(), EPOLL_CTL_ADD, shutdown_fd_, &event) < 0)
    {
        throw std::system_error(errno, std::system_category());
    }
}

void RetransmissionWorker::run()
{
    while (true)
    {
        const int nfds{epoll_wait(epfd_.fd(), events_.data(), epoll_max_events, -1)};
        if (nfds < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                continue;
            }
            throw std::system_error(errno, std::system_category());
        }
        if (!handle_events(nfds))
        {
            break;
        }
    }
}

bool RetransmissionWorker::handle_events(int nfds)
{
    for (std::size_t i = 0; i < static_cast<std::size_t>(nfds); ++i)
    {
        const epoll_event& ev{events_[i]};
        const int client_fd{ev.data.fd};

        if (client_fd == shutdown_fd_)
        {
            return false;
        }

        if ((ev.events & EPOLLIN) != 0)
        {
            process_retransmission_request(client_fd);
        }
    }
    return true;
}

void RetransmissionWorker::process_retransmission_request(int client_fd)
{
    while (auto req_ctx{try_parse_request(client_fd)})
    {
        packet_builder_.reset(req_ctx->request.sequence_num);

        while (packet_builder_.msg_count() < req_ctx->request.msg_count)
        {
            const auto message = Itch::seek_next_message(file_, req_ctx->file_pos_for_retrans);
            if (!message)
            {
                break;
            }

            if (!packet_builder_.try_add_message(*message))
            {
#ifndef DEBUG_NO_NETWORK
                send_packet(req_ctx->client_addr);
#endif
                break;
            }
        }
    }
}

std::optional<RetransmissionWorker::RequestContext> RetransmissionWorker::try_parse_request(int client_fd)
{
    RequestContext ctx{};
    sockaddr_in client_addr{};
    socklen_t client_addr_len{sizeof(client_addr)};

    if (const auto bytes_recv{recvfrom(client_fd,
                                       &ctx.request,
                                       sizeof(ctx.request),
                                       0,
                                       reinterpret_cast<sockaddr*>(&client_addr),
                                       &client_addr_len)};
        bytes_recv < 0)
    {
        if (errno != EWOULDBLOCK)
        {

            std::perror("recvfrom");
        }
        return std::nullopt;
    }
    else if (static_cast<std::size_t>(bytes_recv) != sizeof(ctx.request))
    {
        return std::nullopt;
    }

    ctx.request.msg_count = ntohs(ctx.request.msg_count);
    ctx.request.sequence_num = be64toh(ctx.request.sequence_num);

    const auto file_pos{msg_buffer_->get_file_pos_for_seq(ctx.request.sequence_num)};
    if (!file_pos)
    {
        return std::nullopt;
    }

    ctx.file_pos_for_retrans = *file_pos;
    ctx.client_addr = client_addr;
    return ctx;
}

void RetransmissionWorker::send_packet(sockaddr_in& client_addr)
{
#ifdef DEBUG_NO_NETWORK
    return;
#endif

    packet_builder_.finalize();

    if (const auto bytes_sent{sendto(sock_.fd(),
                                     packet_builder_.cbegin(),
                                     packet_builder_.size(),
                                     0,
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     sizeof(client_addr))};
        bytes_sent < 0)
    {
        std::perror("sendto");
    }
    else if (static_cast<std::size_t>(bytes_sent) != packet_builder_.size())
    {
        std::println(std::cerr,
                     "sendto sent only {} of {} bytes",
                     bytes_sent,
                     packet_builder_.size());
    }
}
