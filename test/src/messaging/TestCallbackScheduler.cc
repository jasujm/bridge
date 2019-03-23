#include "MockCallbackScheduler.hh"

#include <gtest/gtest.h>

using Bridge::Messaging::MockCallbackScheduler;

using namespace std::chrono_literals;
using namespace testing;

namespace {

class MockCallback
{
public:
    MOCK_METHOD1(call, void(int));
};

}

class CallbackSchedulerTest : public testing::Test
{
protected:
    StrictMock<MockCallbackScheduler> callbackScheduler;
    StrictMock<MockCallback> callback;
};

TEST_F(CallbackSchedulerTest, testCallSoon)
{
    auto scheduled_callback = MockCallbackScheduler::Callback {};
    EXPECT_CALL(callbackScheduler,handleCallSoon(_)).
        WillOnce(SaveArg<0>(&scheduled_callback));
    EXPECT_CALL(callback, call(1));
    callbackScheduler.callSoon(&MockCallback::call, std::ref(callback), 1);
    scheduled_callback();
}

TEST_F(CallbackSchedulerTest, testCallLater)
{
    auto scheduled_callback = MockCallbackScheduler::Callback {};
    EXPECT_CALL(callbackScheduler,handleCallLater(123ms, _)).
        WillOnce(SaveArg<1>(&scheduled_callback));
    EXPECT_CALL(callback, call(2));
    callbackScheduler.callLater(123ms, &MockCallback::call, std::ref(callback), 2);
    scheduled_callback();
}
