#include "messaging/Replies.hh"

namespace Bridge {
namespace Messaging {

bool isSuccessful(std::optional<StatusCode> code)
{
    return code && *code >= 0;
}

std::optional<StatusCode> getStatusCode(const Message& statusMessage)
{
    const auto* data = statusMessage.data();
    const auto size = statusMessage.size();
    if (size == sizeof(StatusCode)) {
        auto ret = StatusCode {};
        std::memcpy(&ret, data, size);
        return boost::endian::big_to_native(ret);
    }
    return std::nullopt;
}

}
}
