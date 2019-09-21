#include "messaging/SynchronousExecutionPolicy.hh"
#include "Utility.hh"

#include <array>

namespace Bridge {
namespace Messaging {

void ensureSocketReadable(SynchronousExecutionContext&, SharedSocket socket)
{
    auto& socket_ref = dereference(socket);
    while (!(socket_ref.getsockopt<int>(ZMQ_EVENTS) & ZMQ_POLLIN)) {
        auto item = std::array {
            Pollitem { static_cast<void*>(socket_ref), 0, ZMQ_POLLIN, 0 }
        };
        pollSockets(item);
    }
}

}
}
