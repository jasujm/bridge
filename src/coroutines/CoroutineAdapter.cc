#include "coroutines/CoroutineAdapter.hh"
#include "Utility.hh"

#include <stdexcept>
#include <utility>

namespace Bridge {
namespace Coroutines {

namespace {
void nullResolve() {}
}

Future::Future() : resolveCallback {&nullResolve}
{
}

void Future::resolve()
{
    resolveCallback();
}

class CoroutineAdapter::ClearAwaitVisitor {
public:
    ClearAwaitVisitor(CoroutineAdapter& parent) :
        parent {parent}
    {
    }

    void operator()(const std::shared_ptr<Future>& future)
    {
        dereference(future).resolveCallback = &nullResolve;
    }

    void operator()(const std::shared_ptr<zmq::socket_t>& socket)
    {
        parent.poller.removePollable(dereference(socket));
    }

private:

    CoroutineAdapter& parent;
};

class CoroutineAdapter::PrepareAwaitVisitor {
public:
    PrepareAwaitVisitor(CoroutineAdapter& parent) :
        parent {parent}
    {
    }

    void operator()(std::shared_ptr<Future>& future)
    {
        dereference(future).resolveCallback =
            [this_ = parent.shared_from_this()]()
            {
                dereference(this_).internalResume();
            };
    }

    void operator()(const std::shared_ptr<zmq::socket_t>& socket)
    {
        parent.poller.addPollable(
            socket,
            [socket, this_ = parent.shared_from_this()](zmq::socket_t& socket_)
            {
                if (&socket_ != socket.get()) {
                    throw std::invalid_argument {"Unexpected socket"};
                }
                dereference(this_).internalResume();
            });
    }

private:

    CoroutineAdapter& parent;
};

const CoroutineAdapter::Awaitable* CoroutineAdapter::getAwaited() const
{
    return source ? &awaited : nullptr;
}

void CoroutineAdapter::internalResume()
{
    source();
    const auto this_avoids_selfdestruct = shared_from_this();
    std::visit(ClearAwaitVisitor {*this}, awaited);
    internalUpdate();
}

void CoroutineAdapter::internalUpdate()
{
    if (source) {
        awaited = source.get();
        std::visit(PrepareAwaitVisitor {*this}, awaited);
    }
}

}
}
