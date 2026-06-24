#include "imr/mold/retransmission/feed.h"
#include "imr/mold/retransmission_buffer.h"
#include "imr/mold/types.h"

#include "../../util/binary_io.h"
#include "../io.h"
#include "imr/util/log.h"

#include <arpa/inet.h>
#include <source_location>
#include <stdexcept>
#include <format>
#include <source_location>
#include <system_error>

namespace imr::mold::retransmission
{
    Feed::Feed(const Config& cfg,
               const PacketBuilder::Config& packet_builder_cfg,
               std::span<const char> file,
               const RetransmissionBuffer& retransmission_buffer,
               int shutdown_fd)
        : shutdown_fd_{shutdown_fd},
          packet_builder_(packet_builder_cfg),
          file_{file},
          retransmission_buffer_{&retransmission_buffer}
    {
        // 0 is stdin so will EPERM w/ epoll
        if (shutdown_fd_ <= 0)
        {
            throw std::invalid_argument(std::format("{} invalid shutdown_fd {}",
                                                    std::source_location::current().function_name(),
                                                    shutdown_fd_));
        }
        configure_socket(cfg);

        util::log::debug();
    }

    void Feed::start()
    {
        util::log::info("Retransmission feed: started");

        auto should_stop{false};
        while (!should_stop)
        {
            const int nfds{epoll_wait(epoll_fd_.get(), epoll_events_.data(), num_epoll_events, -1)};

            if (nfds < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    continue;
                }

                util::log::perror();
            }

            for (auto i{0UZ}; i < static_cast<std::size_t>(nfds); ++i)
            {
                const epoll_event& event{epoll_events_[i]};

                if (event.data.fd == shutdown_fd_)
                {
                    should_stop = true;

                    util::log::info("Retransmission feed: shutdown received");
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

                util::log::perror();
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

        std::string_view recv_session(recv_buffer_.data(), sizeof(types::header::Session));
        if (recv_session != packet_builder_.session())
        {
            util::log::debug("Retransmission feed: bad request");
            return std::nullopt;
        }

        RequestContext req_ctx{};

        req_ctx.starting_sequence = util::binary_io::read_at_be<SequenceNumber>(recv_buffer_, sequence_number_offset);

        const std::optional file_pos{retransmission_buffer_->file_position_for(req_ctx.starting_sequence)};
        if (!file_pos)
        {
            util::log::debug("Retransmission feed: requested sequence_number out of range");
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
            const std::span msg{io::read_message(file_, file_pos)};
            // eof / bad file
            if (msg.empty()) [[unlikely]]
            {
                break;
            }

            // packet full before msg_count
            if (!packet_builder_.try_add(msg))
            {
                break;
            }
        }
    }

    void Feed::send_packet([[maybe_unused]] const sockaddr_in& client_addr)
    {
#ifndef DEBUG_NO_NETWORK
        auto msg{packet_builder_.finalize()};

        msghdr send_hdr{};

        send_hdr.msg_name = const_cast<sockaddr_in*>(&client_addr);
        send_hdr.msg_namelen = sizeof(client_addr);
        send_hdr.msg_iov = msg.data();
        send_hdr.msg_iovlen = msg.size();

        if (const auto bytes_sent{sendmsg(socket_.get(), &send_hdr, 0)}; bytes_sent < 0)
        {
            util::log::perror();
        }
#endif
    }

    void Feed::configure_socket([[maybe_unused]] const Config& cfg)
    {
#ifndef DEBUG_NO_NETWORK
        constexpr auto sockopt_on{1};
        if (setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) < 0 ||
            setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEPORT, &sockopt_on, sizeof(sockopt_on)) < 0)
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
            throw std::system_error(errno,
                                    std::system_category(),
                                    std::format("{} inet_pton", std::source_location::current().function_name()));
        }

        if (bind(socket_.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            throw std::system_error(errno,
                                    std::system_category(),
                                    std::format("{} bind", std::source_location::current().function_name()));
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

        util::log::debug();
#endif
    }
}
