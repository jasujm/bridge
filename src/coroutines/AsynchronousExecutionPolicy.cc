#include "coroutines/AsynchronousExecutionPolicy.hh"

namespace Bridge {
namespace Coroutines {

AsynchronousExecutionPolicy::AsynchronousExecutionPolicy(
    Messaging::Poller& poller) :
    poller {&poller}
{
}

AsynchronousExecutionContext::AsynchronousExecutionContext(
    CoroutineAdapter::Sink& sink) :
    sink {&sink}
{
}

void AsynchronousExecutionContext::await(std::shared_ptr<zmq::socket_t> socket)
{
    assert(sink);
    (*sink)(std::move(socket));
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
