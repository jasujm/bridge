#include "messaging/CallbackScheduler.hh"

namespace Bridge {
namespace Messaging {

CallbackScheduler::~CallbackScheduler() = default;

void CallbackScheduler::handleCallSoon(Callback callback)
{
    handleCallLater({}, std::move(callback));
}

}
}
