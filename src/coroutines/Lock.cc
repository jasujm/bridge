#include "coroutines/Lock.hh"

#include "coroutines/AsynchronousExecutionPolicy.hh"
#include "coroutines/Future.hh"

#include <cassert>
#include <utility>

namespace Bridge {
namespace Coroutines {

Mutex::Mutex() : locked {false}, awaitors {}
{
}

void Mutex::lock(AsynchronousExecutionContext& context)
{
    if (locked) {
        auto future_to_await = std::make_shared<Future>();
        awaitors.emplace(future_to_await);
        context.await(std::move(future_to_await));
    } else {
        locked = true;
    }
}

void Mutex::unlock()
{
    if (awaitors.empty()) {
        locked = false;
    } else {
        auto& future_to_resolve = awaitors.front();
        assert(future_to_resolve);
        future_to_resolve->resolve();
        awaitors.pop();
    }
}

Lock::Lock(AsynchronousExecutionContext& context, Mutex& mutex) :
    mutex {mutex}
{
    mutex.lock(context);
}

Lock::~Lock()
{
    mutex.unlock();
}

}
}
