#include <pthread.h>
#include <sched.h>
#include <string>
#include <filesystem>
#include <cstddef>
#include <cstring>
#include <system_error>
#include <thread>
#include <print>
#include <CLI/CLI.hpp>

#include "nasdaq.h"
#include "mold_udp_64.h"
#include "downstream_server.h"

#include "jamutils/M_Map.h"
#include "retransmission_server.h"

int main(int argc, char** argv)
{

    CLI::App cli{"Description to replace"};

    std::string mold_session;
    cli.add_option("mold-session", mold_session, "MoldUDP64 Session")
        ->required()
        ->check([](const std::string& str) {
            if (str.length() != MoldUDP64::session_id_size)
            {
                return "Session must be exactly 10 characters";
            }
            return "";
        });

    std::filesystem::path itch_replay_file;
    cli.add_option("itch-replay-file",
                   itch_replay_file,
                   "Path to TotalView-ITCH replay file in binary")
        ->required()
        ->check(CLI::ExistingFile);

    std::string downstream_mcast_group{"239.0.0.1"};
    cli.add_option("--downstream-mcast-group, --group",
                   downstream_mcast_group,
                   "Downstream multicast group")
        ->capture_default_str();

    int downstream_port{3400};
    cli.add_option("--downstream-port", downstream_port, "Downstream port")
        ->check(CLI::Range(1024, 65535))
        ->capture_default_str();

    int downstream_mcast_ttl{1};
    cli.add_option("--mcast-ttl, --ttl", downstream_mcast_ttl, "Downstream TTL")
        ->check(CLI::Range(0, 255))
        ->capture_default_str();

    bool downstream_mcast_loopback{false};
    cli.add_flag("--loopback", downstream_mcast_loopback, "Enable downstream loopback")
        ->capture_default_str();

    double replay_speed{1};
    cli.add_option("--replay-speed, --speed", replay_speed, "Downstream replay speed")
        ->capture_default_str();

    std::string retrans_address{"127.0.0.1"};
    cli.add_option("--retrans-address",
                   retrans_address,
                   "Retransmission address")
        ->capture_default_str();

    int retrans_port{3500};
    cli.add_option("--retrans-port",
                   retrans_port,
                   "Retransmission port")
        ->check(CLI::Range(1025, 65535))
        ->capture_default_str();

    std::size_t retrans_buffer_size{1U << 22U};
    cli.add_option("--retrans-buffer-size", retrans_buffer_size, "Retransmission buffer size")
        ->capture_default_str();

    auto retrans_threads{std::thread::hardware_concurrency()};
    cli.add_option("--retrans-threads, --threads", retrans_threads, "Number of retransmission threads")
        ->capture_default_str();

    Nasdaq::MarketPhase start_phase{Nasdaq::MarketPhase::pre};
    cli.add_option("--start-phase, --start", start_phase, "Market phase to start replay (pre, open, close)")
        ->transform(
            CLI::CheckedTransformer(Nasdaq::market_phase_map,
                                    CLI::ignore_case))
        ->capture_default_str();

    CLI11_PARSE(cli, argc, argv);

    jam_utils::M_Map file{itch_replay_file, PROT_READ, MAP_PRIVATE | MAP_POPULATE, 0};

    MessageBuffer replay_buffer{retrans_buffer_size};

    RetransmissionServer retrans_server{
        mold_session,
        retrans_address,
        static_cast<std::uint16_t>(retrans_port),
        file.as_span<const std::byte>(),
        &replay_buffer,
        retrans_threads};

    DownstreamServer downstream_server{
        mold_session,
        file.as_span<const std::byte>(),
        &replay_buffer,
        downstream_mcast_group,
        static_cast<std::uint16_t>(downstream_port),
        static_cast<std::uint8_t>(downstream_mcast_ttl),
        downstream_mcast_loopback,
        replay_speed,
        start_phase};

    // run downstream in this thread
    downstream_server.run();
    // here means downstream has finished so signal retransmission to stop
    retrans_server.stop();

    return 0;
}
