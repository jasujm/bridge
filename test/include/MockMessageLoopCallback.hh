#ifndef MOCKMESSAGELOOPCALLBACK_HH_
#define MOCKMESSAGELOOPCALLBACK_HH_

#include <gmock/gmock.h>
#include <zmq.hpp>

namespace Bridge {
namespace Messaging {

class MockMessageLoopCallback {
public:
    MOCK_METHOD1(call, bool(zmq::socket_t&));
};

}
}

#endif // MOCKMESSAGELOOPCALLBACK_HH_
