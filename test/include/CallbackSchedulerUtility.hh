#ifndef CALLBACKSCHEDULERUTILITY_HH_
#define CALLBACKSCHEDULERUTILITY_HH_

#include "main/CallbackScheduler.hh"

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <chrono>
#include <vector>

namespace Bridge {
namespace Main {

inline void pollAndExecuteCallbacks(CallbackScheduler& scheduler)
{
    using namespace std::chrono_literals;
    auto socket = scheduler.getSocket();
    auto pollitems = std::vector {
        zmq::pollitem_t { static_cast<void*>(*socket), 0, ZMQ_POLLIN, 0 }
    };
    const auto count = zmq::poll(pollitems, 100ms);
    ASSERT_EQ(1, count);
    scheduler(*socket);
}


}
}

#endif // CALLBACKSCHEDULERUTILITY_HH_
