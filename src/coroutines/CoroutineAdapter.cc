#include "coroutines/CoroutineAdapter.hh"

#include "coroutines/Future.hh"
#include "messaging/CallbackScheduler.hh"
#include "messaging/Poller.hh"
#include "Utility.hh"

#include <stdexcept>
#include <utility>

namespace Bridge {
namespace Coroutines {

class CoroutineAdapter::ClearAwaitVisitor {
public:
    explicit ClearAwaitVisitor(CoroutineAdapter& parent) :
        parent {parent}
    {
    }

    void operator()(const std::shared_ptr<Future>& future)
    {
        dereference(future).resolveCallback = &Future::nullResolve;
    }

    void operator()(const Messaging::SharedSocket& socket)
    {
        if (const auto poller_ptr = parent.poller.lock()) {
            poller_ptr->removePollable(dereference(socket));
        }
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
                assert(this_);
                if (const auto callback_scheduler_ptr = this_->callbackScheduler.lock()) {
                    callback_scheduler_ptr->callSoon(
                        &CoroutineAdapter::internalResume, this_);
                }
            };
    }

    void operator()(const Messaging::SharedSocket& socket)
    {
        if (const auto poller_ptr = parent.poller.lock()) {
            poller_ptr->addPollable(
                socket,
                [socket, this_ = parent.shared_from_this()](Messaging::Socket& socket_)
                {
                    if (&socket_ != socket.get()) {
                        throw std::invalid_argument {"Unexpected socket"};
                    }
                    assert(this_);
                    this_->internalResume();
                });
        }
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
    [[maybe_unused]] const auto this_avoids_selfdestruct = shared_from_this();
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
