#pragma once
#include <bit>
#include <cstring>
#include <span>
#include <cassert>

#include <ranges>

// Does zero checking outside of debug asserts so you MUST make sure
// the buffer you pass in is big enough to read/write your value
// read/write - at a given position, advancing your position past the value when done
// _at suffix - at an offset, with no advancing position (discarded)
// _be versions: buffer is big endian, your value is little endian

namespace util::binary_io
{

    // basic io which other functions call

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read(std::span<const char> buf, std::size_t& pos)
    {
        assert(pos + sizeof(T) <= buf.size());
        T value{};
        std::memcpy(&value, buf.data() + pos, sizeof(T));
        pos += sizeof(T);
        return value;
    }

    template <typename T>
    concept ByteRange = std::ranges::contiguous_range<T> &&
                        sizeof(std::ranges::range_value_t<T>) == 1;

    // explicitly NOT byte_range here to prevent potential strange overload with the byte_range version of this
    template <typename T>
        requires std::is_trivially_copyable_v<T> && (!ByteRange<T>)
    void write(std::span<char> buf, std::size_t& pos, T value)
    {
        assert(pos + sizeof(T) <= buf.size());
        std::memcpy(buf.data() + pos, &value, sizeof(T));
        pos += sizeof(T);
    }

    void write(std::span<char> buf, std::size_t& pos, const ByteRange auto& data)
    {
        assert(pos + std::ranges::size(data) <= buf.size());
        std::memcpy(buf.data() + pos, std::ranges::data(data), std::ranges::size(data));
        pos += std::ranges::size(data);
    }

    // _at use base functions but we don't care about the advanced position

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_at(std::span<const char> buf, std::size_t offset)
    {
        std::size_t pos = offset;
        return read<T>(buf, pos);
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write_at(std::span<char> buf, std::size_t offset, T value)
    {
        std::size_t pos = offset;
        write(buf, pos, value);
    }

    // _be versions byteswap before reading/writing

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_be(std::span<const char> buf, std::size_t& pos)
    {
        return std::byteswap(read<T>(buf, pos));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write_be(std::span<char> buf, std::size_t& pos, T value)
    {
        write(buf, pos, std::byteswap(value));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T read_at_be(std::span<const char> buf, std::size_t offset)
    {
        return std::byteswap(read_at<T>(buf, offset));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void write_at_be(std::span<char> buf, std::size_t offset, T value)
    {
        write_at(buf, offset, std::byteswap(value));
    }
}
