#include "messaging/Poller.hh"
#include "MockPoller.hh"

#include <gtest/gtest.h>

#include <stdexcept>

using testing::Ref;
using testing::Property;

using Bridge::Messaging::MockPoller;
using Bridge::Messaging::Poller;

namespace {
const auto CALLBACK = [](zmq::socket_t&) {};
}

class PollerTest : public testing::Test {
protected:
    zmq::context_t context;
    Poller::PollableSocket socket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)};
    testing::StrictMock<MockPoller> poller;
};

TEST_F(PollerTest, testAddPollable)
{
    EXPECT_CALL(
        poller,
        handleAddPollable(
            socket,
            Property(&Poller::SocketCallback::target_type, Ref(typeid(CALLBACK)))));
    poller.addPollable(socket, CALLBACK);
}

TEST_F(PollerTest, testTryAddNullSocket)
{
    EXPECT_THROW(poller.addPollable(nullptr, CALLBACK), std::invalid_argument);
}

TEST_F(PollerTest, testTryAddNullCallback)
{
    EXPECT_THROW(poller.addPollable(socket, nullptr), std::invalid_argument);
}

TEST_F(PollerTest, testRemovePollable)
{
    EXPECT_CALL(poller, handleRemovePollable(Ref(*socket)));
    poller.removePollable(*socket);
}
