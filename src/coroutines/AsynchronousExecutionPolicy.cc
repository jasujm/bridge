#include "coroutines/AsynchronousExecutionPolicy.hh"

namespace Bridge {
namespace Coroutines {

AsynchronousExecutionPolicy::AsynchronousExecutionPolicy(
    Messaging::Poller& poller) :
    sink {nullptr},
    poller {&poller}
{
}

void AsynchronousExecutionPolicy::await(
    CoroutineAdapter::AwaitableSocket socket)
{
    dereference(sink)(socket);
}

}
}
