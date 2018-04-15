#include "main/CallbackScheduler.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

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
    scheduler(*socket);
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
    scheduler(*socket);
}

TEST_F(CallbackSchedulerTest, testExceptionRemovesCallbackFromQueue)
{
    EXPECT_CALL(callback, call())
        .WillOnce(testing::Throw(std::runtime_error {"error"}));
    auto socket = scheduler.getSocket();
    scheduler.callOnce([&callback = callback]() { callback.call(); });
    EXPECT_THROW(scheduler(*socket), std::runtime_error);
    scheduler(*socket);
}
