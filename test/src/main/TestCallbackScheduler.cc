#include "main/CallbackScheduler.hh"
#include "CallbackSchedulerUtility.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

using namespace std::chrono_literals;

namespace {

class MockCallback {
public:
    MOCK_METHOD0(call, void());
};

}

class CallbackSchedulerTest : public testing::Test {
protected:
    zmq::context_t context;
    MockCallback callback;
    Bridge::Main::CallbackScheduler scheduler {context};
};

TEST_F(CallbackSchedulerTest, testCallOnce)
{
    EXPECT_CALL(callback, call());
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback]() { callback.call(); });
    pollAndExecuteCallbacks(scheduler);
}

TEST_F(CallbackSchedulerTest, testMultipleCallbacks)
{
    MockCallback callback2;
    {
        testing::InSequence guard;
        EXPECT_CALL(callback, call());
        EXPECT_CALL(callback2, call());
    }
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback]() { callback.call(); });
    scheduler.callOnce([&callback = callback2]() { callback.call(); });
    pollAndExecuteCallbacks(scheduler);
}

TEST_F(CallbackSchedulerTest, testExceptionRemovesCallbackFromQueue)
{
    EXPECT_CALL(callback, call())
        .WillOnce(testing::Throw(std::runtime_error {"error"}));
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback]() { callback.call(); });
    EXPECT_THROW(pollAndExecuteCallbacks(scheduler), std::runtime_error);
}

TEST_F(CallbackSchedulerTest, testDelayedCallback)
{
    EXPECT_CALL(callback, call());
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback]() { callback.call(); }, 50ms);
    const auto tick = std::chrono::steady_clock::now();
    pollAndExecuteCallbacks(scheduler);
    EXPECT_GT(std::chrono::steady_clock::now() - tick, 45ms);
}

TEST_F(CallbackSchedulerTest, testMultipleDelayedCallbacks)
{
    MockCallback callback2;
    {
        testing::InSequence guard;
        EXPECT_CALL(callback, call());
        EXPECT_CALL(callback2, call());
    }
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback]() { callback.call(); }, 20ms);
    scheduler.callOnce([&callback = callback2]() { callback.call(); }, 40ms);
    const auto tick = std::chrono::steady_clock::now();
    pollAndExecuteCallbacks(scheduler);
    pollAndExecuteCallbacks(scheduler);
    EXPECT_LT(std::chrono::steady_clock::now() - tick, 50ms);
}

TEST_F(CallbackSchedulerTest, testMultipleDelayedCallbacksOutOfOrder)
{
    MockCallback callback2;
    {
        testing::InSequence guard;
        EXPECT_CALL(callback, call());
        EXPECT_CALL(callback2, call());
    }
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback2]() { callback.call(); }, 40ms);
    scheduler.callOnce([&callback = callback]() { callback.call(); }, 20ms);
    pollAndExecuteCallbacks(scheduler);
    pollAndExecuteCallbacks(scheduler);
}
