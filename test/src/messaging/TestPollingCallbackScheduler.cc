#include "messaging/PollingCallbackScheduler.hh"
#include "messaging/TerminationGuard.hh"
#include "CallbackSchedulerUtility.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

using namespace std::chrono_literals;
using Bridge::Messaging::TerminationGuard;

namespace {

class MockCallback {
public:
    MOCK_METHOD0(call, void());
};

}

class PollingCallbackSchedulerTest : public testing::Test {
protected:
    zmq::context_t context;
    MockCallback callback;
    Bridge::Messaging::PollingCallbackScheduler scheduler {
        context, TerminationGuard::createTerminationSubscriber(context)};
    TerminationGuard terminationGuard {context};
};

TEST_F(PollingCallbackSchedulerTest, testCallOnce)
{
    EXPECT_CALL(callback, call());
    auto socket = scheduler.getSocket();
    scheduler.callSoon(&MockCallback::call, std::ref(callback));
    pollAndExecuteCallbacks(scheduler);
}

TEST_F(PollingCallbackSchedulerTest, testMultipleCallbacks)
{
    MockCallback callback2;
    {
        testing::InSequence guard;
        EXPECT_CALL(callback, call());
        EXPECT_CALL(callback2, call());
    }
    auto socket = scheduler.getSocket();
    scheduler.callSoon(&MockCallback::call, std::ref(callback));
    scheduler.callSoon(&MockCallback::call, std::ref(callback2));
    pollAndExecuteCallbacks(scheduler);
}

TEST_F(PollingCallbackSchedulerTest, testExceptionRemovesCallbackFromQueue)
{
    EXPECT_CALL(callback, call())
        .WillOnce(testing::Throw(std::runtime_error {"error"}));
    auto socket = scheduler.getSocket();
    scheduler.callSoon(&MockCallback::call, std::ref(callback));
    EXPECT_THROW(pollAndExecuteCallbacks(scheduler), std::runtime_error);
}

TEST_F(PollingCallbackSchedulerTest, testDelayedCallback)
{
    EXPECT_CALL(callback, call());
    auto socket = scheduler.getSocket();
    scheduler.callLater(50ms, &MockCallback::call, std::ref(callback));
    const auto tick = std::chrono::steady_clock::now();
    pollAndExecuteCallbacks(scheduler);
    EXPECT_GT(std::chrono::steady_clock::now() - tick, 45ms);
}

TEST_F(PollingCallbackSchedulerTest, testMultipleDelayedCallbacks)
{
    MockCallback callback2;
    {
        testing::InSequence guard;
        EXPECT_CALL(callback, call());
        EXPECT_CALL(callback2, call());
    }
    auto socket = scheduler.getSocket();
    scheduler.callLater(20ms, &MockCallback::call, std::ref(callback));
    scheduler.callLater(40ms, &MockCallback::call, std::ref(callback2));
    const auto tick = std::chrono::steady_clock::now();
    pollAndExecuteCallbacks(scheduler);
    pollAndExecuteCallbacks(scheduler);
    EXPECT_LT(std::chrono::steady_clock::now() - tick, 50ms);
}

TEST_F(PollingCallbackSchedulerTest, testMultipleDelayedCallbacksOutOfOrder)
{
    MockCallback callback2;
    {
        testing::InSequence guard;
        EXPECT_CALL(callback, call());
        EXPECT_CALL(callback2, call());
    }
    auto socket = scheduler.getSocket();
    scheduler.callLater(40ms, &MockCallback::call, std::ref(callback2));
    scheduler.callLater(20ms, &MockCallback::call, std::ref(callback));
    pollAndExecuteCallbacks(scheduler);
    pollAndExecuteCallbacks(scheduler);
}
