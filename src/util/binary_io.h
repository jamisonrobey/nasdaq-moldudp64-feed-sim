#pragma once

#include <bit>
#include <cstring>
#include <span>
#include <cassert>

// does no checking you must ensure size before calling

namespace util::binary_io
{
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read(std::span<const char> bytes, std::size_t& pos)
    {
        assert(sizeof(T) <= bytes.size());

        T value{};
        std::memcpy(&value, &bytes[pos], sizeof(T));
        pos += sizeof(T);
        return value;
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_be(std::span<const char> bytes, std::size_t& pos)
    {
        T value{read<T>(bytes, pos)};
        return std::byteswap(value);
    }

    // read from 0 index

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read(std::span<const char> bytes)
    {
        assert(sizeof(T) <= bytes.size());

        T value{};
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_be(std::span<const char> bytes)
    {
        T value{read<T>(bytes)};
        return std::byteswap(value);
    }
}
