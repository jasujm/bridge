#include "coroutines/AsynchronousExecutionPolicy.hh"

namespace Bridge {
namespace Coroutines {

AsynchronousExecutionPolicy::AsynchronousExecutionPolicy(
    Messaging::Poller& poller, Messaging::CallbackScheduler& callbackScheduler) :
    poller {&poller},
    callbackScheduler {&callbackScheduler}
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
