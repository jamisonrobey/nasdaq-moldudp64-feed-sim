#include "io.h"

#include "../util/binary_io.h"

#include "imr/mold/types.h"
#include <print>
#include <source_location>

namespace
{
    std::optional<imr::mold::types::LengthPrefix> get_and_check_length_prefix(std::span<const char> bytes, std::size_t& pos)
    {
        using namespace imr::mold;

        if (pos + sizeof(types::LengthPrefix) > bytes.size())
        {
            return std::nullopt;
        }

        const auto length_prefix{imr::util::binary_io::read_be<types::LengthPrefix>(bytes, pos)};

        if (pos + length_prefix > bytes.size())
        {
#ifndef NDEBUG
            std::println(stderr, "{}: length_prefix {} overruns EOF (file position {}, file size {})",
                         std::source_location::current().function_name(),
                         length_prefix,
                         pos,
                         bytes.size());
#endif
            return std::nullopt;
        }

        pos += length_prefix;
        return length_prefix;
    }
}
namespace imr::mold::io
{
    std::span<const char> read_message(std::span<const char> bytes, std::size_t& pos) noexcept
    {
        const auto start{pos};

        if (std::optional length_prefix{get_and_check_length_prefix(bytes, pos)}; length_prefix.has_value())
        {
            static_assert(noexcept(std::span<const char>{}.subspan(0uz, 0uz)), ".subspan() must be implemented as noexcept");
            return bytes.subspan(start, sizeof(types::LengthPrefix) + *length_prefix);
        }

        return {};
    }

    bool skip_message(std::span<const char> bytes, std::size_t& pos) noexcept
    {
        return get_and_check_length_prefix(bytes, pos).has_value();
    }

}
