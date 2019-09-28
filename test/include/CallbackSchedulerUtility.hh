#ifndef CALLBACKSCHEDULERUTILITY_HH_
#define CALLBACKSCHEDULERUTILITY_HH_

#include "messaging/PollingCallbackScheduler.hh"
#include "messaging/Sockets.hh"

#include <gtest/gtest.h>

#include <array>
#include <chrono>

namespace Bridge {
namespace Messaging {

inline void pollAndExecuteCallbacks(PollingCallbackScheduler& scheduler)
{
    using namespace std::chrono_literals;
    auto socket = scheduler.getSocket();
    auto pollitems = std::array {
        Pollitem { socket->handle(), 0, ZMQ_POLLIN, 0 }
    };
    pollSockets(pollitems, 100ms);
    ASSERT_TRUE(pollitems[0].revents & ZMQ_POLLIN);
    scheduler(*socket);
}

}
}

#endif // CALLBACKSCHEDULERUTILITY_HH_
