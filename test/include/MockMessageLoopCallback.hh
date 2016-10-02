#ifndef MOCKMESSAGELOOPCALLBACK_HH_
#define MOCKMESSAGELOOPCALLBACK_HH_

#include <gmock/gmock.h>
#include <zmq.hpp>

namespace Bridge {
namespace Messaging {

class MockMessageLoopCallback {
public:
    MOCK_METHOD1(call, void(zmq::socket_t&));
    MOCK_METHOD0(callSimple, void());
};

}
}

#endif // MOCKMESSAGELOOPCALLBACK_HH_
