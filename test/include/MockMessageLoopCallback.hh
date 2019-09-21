#ifndef MOCKMESSAGELOOPCALLBACK_HH_
#define MOCKMESSAGELOOPCALLBACK_HH_

#include "messaging/Sockets.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Messaging {

class MockMessageLoopCallback {
public:
    MOCK_METHOD1(call, void(Messaging::Socket&));
    MOCK_METHOD0(callSimple, void());
};

}
}

#endif // MOCKMESSAGELOOPCALLBACK_HH_
