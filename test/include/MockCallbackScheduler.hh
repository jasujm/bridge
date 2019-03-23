#include "messaging/CallbackScheduler.hh"

#include <gmock/gmock.h>

namespace Bridge {
namespace Messaging {

class MockCallbackScheduler : public CallbackScheduler
{
public:
    using CallbackScheduler::Callback;
    MOCK_METHOD1(handleCallSoon, void(Callback));
    MOCK_METHOD2(handleCallLater, void(std::chrono::milliseconds, Callback));
};

}
}
