#include "messaging/SynchronousExecutionPolicy.hh"
#include "Utility.hh"

namespace Bridge {
namespace Messaging {

void ensureSocketReadable(
    SynchronousExecutionContext&, std::shared_ptr<zmq::socket_t> socket)
{
    auto& socket_ref = dereference(socket);
    while (!(socket_ref.getsockopt<int>(ZMQ_EVENTS) & ZMQ_POLLIN)) {
        auto item = zmq::pollitem_t {
            static_cast<void*>(socket_ref), 0, ZMQ_POLLIN, 0 };
        static_cast<void>(zmq::poll(&item, 1));
    }
}

}
}
