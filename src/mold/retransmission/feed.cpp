#include "imr/mold/retransmission/feed.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/types.h"

#include "../../util/binary_io.h"
#include "../io.h"

#include <arpa/inet.h>
#include <source_location>
#include <stdexcept>
#include <format>
#include <print>
#include <source_location>
#include <system_error>

namespace imr::mold::retransmission
{
    Feed::Feed(const Config& cfg,
               const PacketBuilder::Config& packet_builder_cfg,
               const RetransmissionBuffer& retransmission_buffer,
               int shutdown_fd)
        : shutdown_fd_{shutdown_fd},
          packet_builder_(packet_builder_cfg),
          retransmission_buffer_{&retransmission_buffer}
    {
        configure_socket(cfg);
    }

    void Feed::start()
    {
        while (true)
        {
            const int nfds{epoll_wait(epoll_fd_.get(), epoll_events_.data(), num_epoll_events, -1)};

            if (nfds < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    continue;
                }
                std::perror("epoll_wait");
            }

            for (auto i{0UZ}; i < static_cast<std::size_t>(nfds); ++i)
            {
                const epoll_event& event{epoll_events_[i]};

                if (event.data.fd == shutdown_fd_)
                {
                    break;
                }

                if ((event.events & EPOLLIN) != 0)
                {
                    handle_request(event.data.fd);
                }
            }
        }
    }

    void Feed::handle_request(int client_fd)
    {
        static socklen_t client_addr_len{sizeof(RequestContext::client_address)};

        sockaddr_in client_addr{};

        if (const auto bytes_recv{recvfrom(client_fd,
                                           recv_buffer_.data(),
                                           sizeof(recv_buffer_),
                                           0,
                                           reinterpret_cast<sockaddr*>(&client_addr),
                                           &client_addr_len)};
            bytes_recv < 0)
        {
            if (errno != EWOULDBLOCK)
            {

                std::perror("recvfrom");
            }
            return;
        }
        else if (bytes_recv != mold::types::header::length)
        {
            return;
        }

        const std::optional<RequestContext> req_ctx{parse_request(std::move(client_addr))};

        if (req_ctx)
        {
            build_packet(*req_ctx);
            send_packet(req_ctx->client_address);
        }
    }

    std::optional<Feed::RequestContext> Feed::parse_request(sockaddr_in&& client_addr)
    {
        using namespace types::header;

        std::string_view recv_session(recv_buffer_.data(), session_offset);
        if (recv_session != packet_builder_.session())
        {
            return std::nullopt;
        }

        RequestContext req_ctx{};

        req_ctx.starting_sequence = util::binary_io::read_at_be<SequenceNumber>(recv_buffer_, sequence_number_offset);

        const std::optional file_pos{retransmission_buffer_->file_position_for(req_ctx.starting_sequence)};
        if (!file_pos)
        {
            return std::nullopt;
        }

        req_ctx.file_position_for_retransmission = *file_pos;

        req_ctx.msg_count = util::binary_io::read_at_be<MessageCount>(recv_buffer_, message_count_offset);

        req_ctx.client_address = std::move(client_addr);

        return req_ctx;
    }

    void Feed::build_packet(const RequestContext& req_ctx)
    {
        std::size_t file_pos{req_ctx.file_position_for_retransmission};

        packet_builder_.reset(req_ctx.starting_sequence);

        for (auto i{0UZ}; i < req_ctx.msg_count; ++i)
        {
            const std::optional msg{io::read_message(file_, file_pos)};
            if (!msg) [[unlikely]]
            {
                break;
            }

            if (!packet_builder_.try_add(*msg))
            {
                // packet full before msg_count
                break;
            }
        }
    }

    void Feed::send_packet(const sockaddr_in& client_addr)
    {
        [[maybe_unused]]
        std::span msg{packet_builder_.finalize()};
#ifndef DEBUG_NO_NETWORK
        if (const auto bytes_sent{sendto(socket_.get(),
                                         msg.data(),
                                         msg.size(),
                                         0,
                                         reinterpret_cast<const sockaddr*>(&client_addr),
                                         sizeof(client_addr))};
            bytes_sent < 0)
        {
            std::perror("sendto");
        }
        else if (static_cast<std::size_t>(bytes_sent) != msg.size())
        {
#ifndef NDEBUG
            std::println(stderr, "{} sendto: only sent {} of {} bytes",
                         std::source_location::current().function_name(), bytes_sent, msg.size());
#endif
        }
#endif
    }

    void Feed::configure_socket(const Config& cfg)
    {
        constexpr auto sockopt_on{1};
        if (setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEPORT, &sockopt_on, sizeof(sockopt_on)) < 0 ||
            setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = std::byteswap(cfg.port);

        if (const auto ret{inet_pton(AF_INET, cfg.address.data(), &addr.sin_addr)}; ret == 0)
        {
            throw std::invalid_argument(std::format("{} Feed::Config::address invalid ip format",
                                                    std::source_location::current().function_name()));
        }
        else if (ret < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = socket_.get();

        if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, socket_.get(), &event) < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        event.data.fd = shutdown_fd_;

        if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, shutdown_fd_, &event) < 0)
        {
            throw std::system_error(errno, std::system_category());
        }
    }
}
