#ifndef RETRANSMISSION_WORKER_H
#define RETRANSMISSION_WORKER_H

#include "jamutils/FD.h"
#include "retransmission_buffer.h"
#include "mold_udp_64.h"
#include "packet_builder.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <string_view>
#include <cstdint>
#include <span>
#include <optional>

inline constexpr auto epoll_max_events{1024};

class RetransmissionWorker
{
  public:
    RetransmissionWorker(std::string_view session,
                         std::string_view address,
                         std::uint16_t port,
                         int shutdown_fd,
                         std::span<const std::byte> file,
                         RetransmissionBuffer* retrans_buffer);

    void run();

  private:
    PacketBuilder packet_builder_;
    int shutdown_fd_;
    std::span<const std::byte> file_;
    RetransmissionBuffer* retrans_buffer_;

    jam_utils::FD sock_;
    jam_utils::FD epfd_;
    std::array<epoll_event, epoll_max_events> events_{};
    sockaddr_in server_addr_{};

    struct RequestContext
    {
        MoldUDP64::RetransmissionRequest request;
        std::size_t file_pos_for_retrans{};
        sockaddr_in client_addr{};
    };

    bool handle_events(int nfds);
    void process_retransmission_request(int client_fd);
    std::optional<RequestContext> try_parse_request(int client_fd);
    void send_packet(sockaddr_in& client_addr);
};

#endif
