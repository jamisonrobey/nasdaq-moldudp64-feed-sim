#include "io.h"

#include "../util/binary_io.h"

#include "types.h"

namespace mold::io
{
    std::span<const char> read_message(std::span<const char> bytes, std::size_t& pos)
    {
        if (pos + sizeof(LengthPrefix) > bytes.size())
        {
            return {};
        }

        const auto start{pos};

        const auto length_prefix{util::binary_io::read_be<LengthPrefix>(bytes, pos)};
        if (pos + length_prefix > bytes.size())
        {
            pos = start;
            return {};
        }

        pos += length_prefix;
        return bytes.subspan(start, sizeof(LengthPrefix) + length_prefix);
    }
}
