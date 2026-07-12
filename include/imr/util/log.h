#pragma once
#include <cstring>
#include <format>
#include <print>
#include <source_location>
#include <unistd.h>
namespace imr::util::log
{
    namespace detail
    {
#ifndef IMR_LOG_LEVEL
#define IMR_LOG_LEVEL 0
#endif
        inline constexpr int level{IMR_LOG_LEVEL};

        inline constexpr std::string_view color_error{"\033[1;31m"};
        inline constexpr std::string_view color_warn{"\033[1;33m"};
        inline constexpr std::string_view color_info{"\033[36m"};
        inline constexpr std::string_view color_debug{"\033[35m"};

        template <typename... Args>
        void base(std::FILE* stream, std::string_view tag, std::string_view color, std::format_string<Args...> fmt, Args&&... args)
        {
            if (static_cast<bool>(isatty(fileno(stream))))
                std::print(stream, "{}[imr:{}]\033[0m ", color, tag);
            else
                std::print(stream, "[imr:{}] ", tag);
            std::println(stream, fmt, std::forward<Args>(args)...);
        }
    }
    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args)
    {
        if constexpr (detail::level >= 0)
        {
            detail::base(stderr, "error", detail::color_error, fmt, std::forward<Args>(args)...);
        }
    }
    // mimics std::perror but never any msg other than loc.function_name
    inline void perror(std::source_location loc = std::source_location::current())
    {
        if constexpr (detail::level >= 0)
        {
            error("{}: {}", loc.function_name(), std::strerror(errno));
        }
    }
    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args)
    {
        if constexpr (detail::level >= 1)
        {
            detail::base(stdout, "warn", detail::color_warn, fmt, std::forward<Args>(args)...);
        }
    }
    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args)
    {
        if constexpr (detail::level >= 2)
        {
            detail::base(stdout, "info", detail::color_info, fmt, std::forward<Args>(args)...);
        }
    }
    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args)
    {
        if constexpr (detail::level >= 3)
        {
            detail::base(stdout, "debug", detail::color_debug, fmt, std::forward<Args>(args)...);
        }
    }
    template <typename... Args>
    void debug(std::source_location loc = std::source_location::current())
    {
        if constexpr (detail::level >= 3)
        {
            debug("{}", loc.function_name());
        }
    }
}
