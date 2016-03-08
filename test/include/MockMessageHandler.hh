#ifndef MOCKMESSAGEHANDLER_HH_
#define MOCKMESSAGEHANDLER_HH_

#include "messaging/MessageHandler.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Messaging {

class MockMessageHandler : public MessageHandler {
public:
    MOCK_METHOD3(
        doHandle, bool(const std::string&, ParameterRange, OutputSink));
};

}
}

#endif // MOCKMESSAGEHANDLER_HH_
