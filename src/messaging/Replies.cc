#include "messaging/Replies.hh"

namespace Bridge {
namespace Messaging {

bool isSuccessful(std::optional<StatusCode> code)
{
    return code && *code >= 0;
}

}
}
