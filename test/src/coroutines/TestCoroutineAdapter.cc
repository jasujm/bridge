#include "coroutines/CoroutineAdapter.hh"
#include "coroutines/Future.hh"
#include "messaging/Sockets.hh"
#include "Utility.hh"
#include "MockCallbackScheduler.hh"
#include "MockPoller.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using Bridge::dereference;
using Bridge::Coroutines::CoroutineAdapter;
using Bridge::Coroutines::Future;
using namespace Bridge::Messaging;

using testing::_;
using testing::Mock;
using testing::SaveArg;
using testing::StrictMock;

namespace {

template<typename Awaitable>
void expectAwaits(CoroutineAdapter& adapter, const Awaitable& awaitable)
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

    MessageContext context;
    std::shared_ptr<StrictMock<MockPoller>> poller {
        std::make_shared<StrictMock<MockPoller>>()};
    std::shared_ptr<StrictMock<MockCallbackScheduler>> callbackScheduler {
        std::make_shared<StrictMock<MockCallbackScheduler>>()};
};

TEST_F(CoroutineAdapterTest, testFutureCoroutine)
{
    auto futures = std::vector {
        std::make_shared<Future>(),
        std::make_shared<Future>(),
    };
    auto callback = MockCallbackScheduler::Callback {};
    EXPECT_CALL(*callbackScheduler, handleCallSoon(_))
        .WillOnce(SaveArg<0>(&callback));
    auto coroutineAdapter = createCoroutineAdapter(
        futures.begin(), futures.end());
    expectAwaits(*coroutineAdapter, futures.front());
    futures.front()->resolve();
    Mock::VerifyAndClearExpectations(poller.get());
    EXPECT_CALL(*callbackScheduler, handleCallSoon(_))
        .WillOnce(SaveArg<0>(&callback));
    callback();
    expectAwaits(*coroutineAdapter, futures.back());
    futures.back()->resolve();
    Mock::VerifyAndClearExpectations(poller.get());
    callback();
    EXPECT_FALSE(coroutineAdapter->getAwaited());
}

TEST_F(CoroutineAdapterTest, testSocketCoroutine)
{
    auto sockets = std::vector {
        makeSharedSocket(context, SocketType::pair),
        makeSharedSocket(context, SocketType::pair),
    };
    auto callback = MockPoller::SocketCallback {};
    EXPECT_CALL(*poller, handleAddPollable(sockets.front(), _))
        .WillOnce(SaveArg<1>(&callback));
    auto coroutineAdapter = createCoroutineAdapter(
        sockets.begin(), sockets.end());
    expectAwaits(*coroutineAdapter, sockets.front());
    Mock::VerifyAndClearExpectations(poller.get());
    EXPECT_CALL(*poller, handleAddPollable(sockets.back(), _))
        .WillOnce(SaveArg<1>(&callback));
    EXPECT_CALL(*poller, handleRemovePollable(testing::Ref(*sockets.front())));
    callback(*sockets.front());
    expectAwaits(*coroutineAdapter, sockets.back());
    Mock::VerifyAndClearExpectations(poller.get());
    EXPECT_CALL(*poller, handleRemovePollable(testing::Ref(*sockets.back())));
    callback(*sockets.back());
    EXPECT_FALSE(coroutineAdapter->getAwaited());
}
