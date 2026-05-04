#pragma once

#include <bit>
#include <cstring>
#include <span>
#include <cassert>
#include <string_view>

// does no checking you must ensure size before calling
// read/write() updates position passed in
// _at() functions do not update the position passed in
// _be() functions will convert to or from big endian depending on the action

namespace util::binary_io
{
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read(std::span<const char> bytes, std::size_t& pos)
    {
        assert(pos + sizeof(T) <= bytes.size());
        T value{};
        std::memcpy(&value, &bytes[pos], sizeof(T));
        pos += sizeof(T);
        return value;
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_be(std::span<const char> bytes, std::size_t& pos)
    {
        return std::byteswap(read<T>(bytes, pos));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_at(std::span<const char> bytes, std::size_t offset)
    {
        assert(offset + sizeof(T) <= bytes.size());
        T value{};
        std::memcpy(&value, &bytes[offset], sizeof(T));
        return value;
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_at_be(std::span<const char> bytes, std::size_t offset)
    {
        return std::byteswap(read_at<T>(bytes, offset));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write(std::span<char> bytes, std::size_t& pos, T value)
    {
        assert(pos + sizeof(T) <= bytes.size());
        std::memcpy(&bytes[pos], &value, sizeof(T));
        pos += sizeof(T);
    }

    inline void write(std::span<char> bytes, std::size_t& pos, std::string_view data)
    {
        assert(pos + data.size() <= bytes.size());
        std::memcpy(&bytes[pos], data.data(), data.size());
        pos += data.size();
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write_be(std::span<char> bytes, std::size_t& pos, T value)
    {
        write(bytes, pos, std::byteswap(value));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write_at(std::span<char> bytes, std::size_t offset, T value)
    {
        assert(offset + sizeof(T) <= bytes.size());
        std::memcpy(&bytes[offset], &value, sizeof(T));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write_at_be(std::span<char> bytes, std::size_t offset, T value)
    {
        write_at(bytes, offset, std::byteswap(value));
    }
}
