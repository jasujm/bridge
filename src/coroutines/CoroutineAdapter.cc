#include "coroutines/CoroutineAdapter.hh"
#include "Utility.hh"

#include <stdexcept>
#include <utility>

namespace Bridge {
namespace Coroutines {

CoroutineAdapter::AwaitableSocket CoroutineAdapter::getAwaitedSocket() const
{
    return awaitedSocket;
}

void CoroutineAdapter::operator()(zmq::socket_t& socket)
{
    if (awaitedSocket.get() != &socket) {
        throw std::invalid_argument {"Socket was not awaited"};
    }
    source();
    const auto this_avoids_selfdestruct = shared_from_this();
    poller.removePollable(dereference(awaitedSocket));
    internalUpdateSocket();
    if (!source) {
        awaitedSocket.reset();
    }
}

void CoroutineAdapter::internalUpdateSocket()
{
    if (source) {
        awaitedSocket = source.get();
        poller.addPollable(
            awaitedSocket,
            [this_ = shared_from_this()](zmq::socket_t& socket)
            {
                dereference(this_)(socket);
            });
    }
}

}
}
