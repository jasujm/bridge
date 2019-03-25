#include "coroutines/CoroutineAdapter.hh"
#include "coroutines/Future.hh"
#include "Utility.hh"
#include "MockCallbackScheduler.hh"
#include "MockPoller.hh"

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <algorithm>
#include <vector>

using Bridge::dereference;
using Bridge::Coroutines::CoroutineAdapter;
using Bridge::Coroutines::Future;
using Bridge::Messaging::MockCallbackScheduler;
using Bridge::Messaging::MockPoller;

using testing::_;
using testing::Mock;
using testing::SaveArg;

namespace {

void expectAwaits(CoroutineAdapter& adapter, const auto& awaitable)
{
    EXPECT_EQ(
        CoroutineAdapter::Awaitable {awaitable},
        dereference(adapter.getAwaited()));
}

}

class CoroutineAdapterTest : public testing::Test
{
protected:

    template<typename AwaitableIterator>
    auto createCoroutineAdapter(
    AwaitableIterator first, AwaitableIterator last)
    {
        return CoroutineAdapter::create(
            [first, last](auto& sink) { std::copy(first, last, begin(sink)); },
            poller, callbackScheduler);
    }

    zmq::context_t context;
    testing::StrictMock<MockPoller> poller;
    testing::StrictMock<MockCallbackScheduler> callbackScheduler;
};

TEST_F(CoroutineAdapterTest, testFutureCoroutine)
{
    auto futures = std::vector {
        std::make_shared<Future>(),
        std::make_shared<Future>(),
    };
    auto callback = MockCallbackScheduler::Callback {};
    EXPECT_CALL(callbackScheduler, handleCallSoon(_))
        .WillOnce(SaveArg<0>(&callback));
    auto coroutineAdapter = createCoroutineAdapter(
        futures.begin(), futures.end());
    expectAwaits(*coroutineAdapter, futures.front());
    futures.front()->resolve();
    Mock::VerifyAndClearExpectations(&poller);
    EXPECT_CALL(callbackScheduler, handleCallSoon(_))
        .WillOnce(SaveArg<0>(&callback));
    callback();
    expectAwaits(*coroutineAdapter, futures.back());
    futures.back()->resolve();
    Mock::VerifyAndClearExpectations(&poller);
    callback();
    EXPECT_FALSE(coroutineAdapter->getAwaited());
}

TEST_F(CoroutineAdapterTest, testSocketCoroutine)
{
    auto sockets = std::vector {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair),
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair),
    };
    auto callback = MockPoller::SocketCallback {};
    EXPECT_CALL(poller, handleAddPollable(sockets.front(), _))
        .WillOnce(SaveArg<1>(&callback));
    auto coroutineAdapter = createCoroutineAdapter(
        sockets.begin(), sockets.end());
    expectAwaits(*coroutineAdapter, sockets.front());
    Mock::VerifyAndClearExpectations(&poller);
    EXPECT_CALL(poller, handleAddPollable(sockets.back(), _))
        .WillOnce(SaveArg<1>(&callback));
    EXPECT_CALL(poller, handleRemovePollable(testing::Ref(*sockets.front())));
    callback(*sockets.front());
    expectAwaits(*coroutineAdapter, sockets.back());
    Mock::VerifyAndClearExpectations(&poller);
    EXPECT_CALL(poller, handleRemovePollable(testing::Ref(*sockets.back())));
    callback(*sockets.back());
    EXPECT_FALSE(coroutineAdapter->getAwaited());
}
