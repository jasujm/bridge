#include "coroutines/AsynchronousExecutionPolicy.hh"

namespace Bridge {
namespace Coroutines {

AsynchronousExecutionPolicy::AsynchronousExecutionPolicy(
    std::weak_ptr<Messaging::Poller> poller,
    std::weak_ptr<Messaging::CallbackScheduler> callbackScheduler) :
    poller {std::move(poller)},
    callbackScheduler {std::move(callbackScheduler)}
{
}

AsynchronousExecutionContext::AsynchronousExecutionContext(
    CoroutineAdapter::Sink& sink) :
    sink {&sink}
{
}

void ensureSocketReadable(
    AsynchronousExecutionContext& context,
    std::shared_ptr<zmq::socket_t> socket)
{
    while (!(dereference(socket).getsockopt<int>(ZMQ_EVENTS) & ZMQ_POLLIN)) {
        context.await(socket);
    }
}

}
}
