#pragma once

#include <utility>

namespace imr::util
{
    inline constexpr bool is_debug_build =
#ifdef NDEBUG
        false;
#else
        true;
#endif

    template <typename T, bool Active = is_debug_build>
    class DebugField;

    template <typename T>
    class DebugField<T, false>
    {
      public:
        template <typename... Args>
        constexpr explicit DebugField(Args&&...) noexcept {}

        T& operator*() = delete;
        T* operator->() = delete;
    };

    template <typename T>
    class DebugField<T, true>
    {
        T value_;

      public:
        template <typename... Args>
        constexpr explicit DebugField(Args&&... args)
            : value_(std::forward<Args>(args)...) {}

        [[nodiscard]] constexpr T& operator*() & noexcept { return value_; }
        [[nodiscard]] constexpr T const& operator*() const& noexcept { return value_; }
        [[nodiscard]] constexpr T&& operator*() && noexcept { return std::move(value_); }
        [[nodiscard]] constexpr T* operator->() noexcept { return &value_; }
        [[nodiscard]] constexpr T const* operator->() const noexcept { return &value_; }
    };
}
