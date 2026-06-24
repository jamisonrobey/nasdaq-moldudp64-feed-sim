#include "imr/server.h"
#include "imr/mold/retransmission_buffer.h"

namespace imr
{
    Server::Server(const Config& cfg)
        : mapped_itch_file_(cfg.mapped_itch_file_cfg),
          retransmission_buffer_(cfg.retransmission_buffer_size),
          downstream_feed_(cfg.downstream_feed_config,
                           cfg.packet_builder_cfg,
                           mapped_itch_file_.as_span(),
                           retransmission_buffer_),
          retransmission_feeds_(cfg.num_retransmission_feeds,
                                cfg.retransmission_feed_config,
                                cfg.packet_builder_cfg,
                                mapped_itch_file_.as_span(),
                                retransmission_buffer_)
    {}

    void Server::start()
    {
        downstream_thread_ = std::jthread([this](std::stop_token st) {
            // downstream blocks till finished
            downstream_feed_.start(st);
            retransmission_feeds_.stop();
        });
    }

    void Server::wait_for_downstream()
    {
        if (downstream_thread_.joinable())
        {
            downstream_thread_.join();
        }
    }

    void Server::stop()
    {
        downstream_thread_.request_stop();
    }

    Server::~Server()
    {
        downstream_thread_.request_stop();
        if (downstream_thread_.joinable())
        {
            downstream_thread_.join();
        }
        retransmission_feeds_.stop();
    }

    std::expected<std::unique_ptr<Server>, std::string> make_server(const Server::Config& cfg) noexcept
    {
        try
        {
            return std::make_unique<Server>(cfg);
        }
        catch (const std::exception& ex)
        {
            return std::unexpected(ex.what());
        }
    }
}
