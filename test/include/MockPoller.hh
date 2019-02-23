#include "messaging/Poller.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Messaging {

class MockPoller : public Poller {
public:
    MOCK_METHOD2(handleAddPollable, void(PollableSocket, SocketCallback));
    MOCK_METHOD1(handleRemovePollable, void(zmq::socket_t&));
};

}
}
