#include "coroutines/CoroutineAdapter.hh"
#include "MockPoller.hh"

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <algorithm>
#include <stdexcept>
#include <vector>

using Bridge::Coroutines::CoroutineAdapter;
using Bridge::Messaging::MockPoller;

template<typename SocketIterator>
auto createCoroutineAdapter(
    SocketIterator first, SocketIterator last, MockPoller& poller)
{
    return CoroutineAdapter::create(
        [first, last](auto& sink) { std::copy(first, last, begin(sink)); },
        poller);
}

class CoroutineAdapterTest : public testing::Test
{
protected:
    zmq::context_t context;
    std::vector<CoroutineAdapter::AwaitableSocket> sockets {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair),
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair),
    };
    testing::NiceMock<MockPoller> poller;
    std::shared_ptr<CoroutineAdapter> coroutineAdapter {
        createCoroutineAdapter(sockets.begin(), sockets.end(), poller) };
};

TEST_F(CoroutineAdapterTest, testCoroutine)
{
    EXPECT_EQ(sockets.front(), coroutineAdapter->getAwaitedSocket());
    (*coroutineAdapter)(*sockets.front());
    EXPECT_EQ(sockets.back(), coroutineAdapter->getAwaitedSocket());
    (*coroutineAdapter)(*sockets.back());
    EXPECT_EQ(nullptr, coroutineAdapter->getAwaitedSocket());
}

TEST_F(CoroutineAdapterTest, testImmediatelyCompletingCoroutine)
{
    auto adapter = createCoroutineAdapter(
        sockets.begin(), sockets.begin(), poller);
    EXPECT_EQ(nullptr, adapter->getAwaitedSocket());
}

TEST_F(CoroutineAdapterTest, testCoroutineAwaitWrongSocket)
{
    EXPECT_THROW((*coroutineAdapter)(*sockets.back()), std::invalid_argument);
}

TEST_F(CoroutineAdapterTest, testPollerInteraction)
{
    using namespace testing;
    StrictMock<MockPoller> poller;
    coroutineAdapter.reset();
    EXPECT_CALL(poller, handleAddPollable(sockets.front(), _));
    coroutineAdapter = createCoroutineAdapter(
        sockets.begin(), sockets.end(), poller);
    Mock::VerifyAndClearExpectations(&poller);
    EXPECT_CALL(poller, handleAddPollable(sockets.back(), _));
    EXPECT_CALL(poller, handleRemovePollable(Ref(*sockets.front())));
    (*coroutineAdapter)(*sockets.front());
    Mock::VerifyAndClearExpectations(&poller);
    EXPECT_CALL(poller, handleRemovePollable(Ref(*sockets.back())));
    (*coroutineAdapter)(*sockets.back());
}
