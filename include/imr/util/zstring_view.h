#pragma once

#include <string_view>
#include <string>

namespace imr::util
{
    // basic wrapper that just ensures null terminator for c api that don't take length of str
    class zstring_view
    {
      public:
        constexpr zstring_view(const char* str) noexcept : sv_(str) {}

        zstring_view(const std::string& str) noexcept : sv_(str) {}

        zstring_view(std::string_view) = delete;
        zstring_view(std::string&&) = delete;

        constexpr const char* c_str() const noexcept { return sv_.data(); }
        constexpr std::string_view sv() const noexcept { return sv_; }
        constexpr const char* data() const noexcept { return sv_.data(); }
        constexpr std::size_t size() const noexcept { return sv_.size(); }
        constexpr bool empty() const noexcept { return sv_.empty(); }
        constexpr operator std::string_view() const noexcept { return sv_; }

      private:
        std::string_view sv_;
    };
}
