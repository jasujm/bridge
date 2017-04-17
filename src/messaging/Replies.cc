#include "messaging/Replies.hh"

namespace Bridge {
namespace Messaging {

bool isSuccessful(boost::optional<StatusCode> code)
{
    return code && *code >= 0;
}

}
}
