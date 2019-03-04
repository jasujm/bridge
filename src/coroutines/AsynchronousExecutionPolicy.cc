#include "coroutines/AsynchronousExecutionPolicy.hh"

namespace Bridge {
namespace Coroutines {

AsynchronousExecutionPolicy::AsynchronousExecutionPolicy(
    Messaging::Poller& poller) :
    poller {&poller}
{
}

AsynchronousExecutionPolicy::Context::Context(CoroutineAdapter::Sink& sink) :
    sink {&sink}
{
}

void AsynchronousExecutionPolicy::Context::await(
    CoroutineAdapter::AwaitableSocket socket)
{
    assert(sink);
    (*sink)(std::move(socket));
}

}
}
