#include "io.h"

#include "../util/binary_io.h"

#include "imr/mold/types.h"
#include <print>
#include <source_location>

namespace imr::mold::io
{
    std::span<const char> read_message(std::span<const char> bytes, std::size_t& pos) noexcept
    {
        if (pos + sizeof(types::LengthPrefix) > bytes.size())
        {
            return {};
        }

        const auto start{pos};

        const auto length_prefix{util::binary_io::read_be<types::LengthPrefix>(bytes, pos)};
        if (pos + length_prefix > bytes.size())
        {
#ifndef NDEBUG
            std::println(stderr, "{}: length_prefix {} overruns EOF (file position {}, file size {})",
                         std::source_location::current().function_name(),
                         length_prefix,
                         pos,
                         bytes.size());
#endif
            pos = start;
            return {};
        }

        pos += length_prefix;

        // not required to be noexcept by the standard, but is in gcc/clang (supported compilers)
        static_assert(noexcept(std::span<const char>{}.subspan(0uz, 0uz)), ".subspan() must be implemented as noexcept");
        return bytes.subspan(start, sizeof(types::LengthPrefix) + length_prefix);
    }
}
