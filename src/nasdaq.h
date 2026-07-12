#ifndef NASDAQ_H
#define NASDAQ_H

#include <chrono>
#include <map>
#include <string>

namespace Nasdaq
{

enum class MarketPhase
{
    pre,
    open,
    close
};

constexpr std::chrono::nanoseconds market_phase_to_timestamp(const MarketPhase& phase)
{
    using namespace std::chrono_literals;
    switch (phase)
    {
    case MarketPhase::pre:
        return 0ns;
    case MarketPhase::open:
        return 9h + 30min;
    case MarketPhase::close:
        return 16h;
    default:
        return 0ns;
    }
}

// for CLI11 (https://github.com/CLIUtils/CLI11/blob/main/examples/enum.cpp)
const std::map<std::string, MarketPhase> market_phase_map{{"pre", MarketPhase::pre},
                                                          {"open", MarketPhase::open},
                                                          {"close", MarketPhase::close}};
}

#endif