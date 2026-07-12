#include "mold_udp_64.h"

#include <endian.h>
#include <netdb.h>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <format>

using namespace MoldUDP64;

DownstreamHeader::DownstreamHeader(std::string_view session_)
{
    if (session_.length() != session_id_size)
    {
        throw std::invalid_argument(std::format("session {} has length {} expected {} ",
                                                session_,
                                                session_.length(),
                                                session_id_size));
    }

    std::memcpy(&session, session_.data(), session_.size());
}

DownstreamHeader::DownstreamHeader(const Session& session_)
    : session{session_}
{
}
