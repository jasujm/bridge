#include "messaging/MessageLoop.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Sockets.hh"
#include "Blob.hh"
#include "MockMessageLoopCallback.hh"

#include <boost/range/combine.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <csignal>
#include <string>
#include <stdexcept>

using namespace Bridge::BlobLiterals;
using namespace Bridge::Messaging;
using namespace std::string_view_literals;

using testing::_;
using testing::Ref;
using testing::Invoke;

namespace {
constexpr auto N_SOCKETS = 2;
const auto DEFAULT_MSG = "default"_BS;
const auto OTHER_MSG = "other"_BS;
constexpr auto ENDPOINTS = std::array {
    "inproc://endpoint1"sv,
    "inproc://endpoint2"sv,
};
}

class MessageLoopTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (auto&& t : boost::combine(ENDPOINTS, frontSockets, backSockets, callbacks)) {
            ON_CALL(t.get<3>(), call(_)).WillByDefault(
                Invoke(
                    [this](auto& socket)
                    {
                        auto msg = Message {};
                        recvMessage(socket, msg);
                        EXPECT_EQ(DEFAULT_MSG, messageView(msg));
                        EXPECT_FALSE(msg.more());
                        std::raise(SIGTERM);
                    }));
            bindSocket(*t.get<2>(), t.get<0>());
            connectSocket(t.get<1>(), t.get<0>());
            loop.addPollable(
                t.get<2>(),
                [&callback = t.get<3>()](auto& socket)
                {
                    callback.call(socket);
                });
        }
    }

    MessageContext context;
    std::array<Socket, N_SOCKETS> frontSockets {
        Socket {context, SocketType::dealer},
        Socket {context, SocketType::dealer},
    };
    std::array<SharedSocket, N_SOCKETS> backSockets {
        makeSharedSocket(context, SocketType::dealer),
        makeSharedSocket(context, SocketType::dealer),
    };
    std::array<
        testing::StrictMock<MockMessageLoopCallback>,
        N_SOCKETS> callbacks;
    MessageLoop loop {context};
};

TEST_F(MessageLoopTest, testSingleMessage)
{
    EXPECT_CALL(callbacks[0], call(Ref(*backSockets[0])));
    sendMessage(frontSockets[0], messageBuffer(DEFAULT_MSG));
    loop.run();
}

TEST_F(MessageLoopTest, testMultipleMessages)
{
    EXPECT_CALL(callbacks[0], call(Ref(*backSockets[0])))
        .WillOnce(
            Invoke(
                [this](auto& socket)
                {
                    auto msg = Message {};
                    recvMessage(socket, msg);
                    EXPECT_EQ(OTHER_MSG, messageView(msg));
                    EXPECT_FALSE(msg.more());
                    sendMessage(frontSockets[1], messageBuffer(DEFAULT_MSG));
                }));
    EXPECT_CALL(callbacks[1], call(Ref(*backSockets[1])));
    sendMessage(frontSockets[0], messageBuffer(OTHER_MSG));
    loop.run();
}

TEST_F(MessageLoopTest, testTerminate)
{
    EXPECT_CALL(callbacks[0], call(Ref(*backSockets[0])))
        .WillOnce(
            Invoke(
                [this](auto& socket)
                {
                    auto msg = Message {};
                    recvMessage(socket, msg);
                    EXPECT_EQ(OTHER_MSG, messageView(msg));
                    EXPECT_FALSE(msg.more());
                    std::raise(SIGTERM);
                }));
    EXPECT_CALL(callbacks[1], call(Ref(*backSockets[1]))).Times(0);
    auto termination_subscriber = loop.createTerminationSubscriber();
    sendMessage(frontSockets[0], messageBuffer(OTHER_MSG));
    loop.run();
    auto msg = Message {};
    EXPECT_TRUE(recvMessageNonblocking(termination_subscriber, msg));
}

TEST_F(MessageLoopTest, testRemove)
{
    loop.removePollable(*backSockets[0]);
    sendMessage(frontSockets[0], messageBuffer(DEFAULT_MSG));
    sendMessage(frontSockets[1], messageBuffer(DEFAULT_MSG));
    EXPECT_CALL(callbacks[0], call(_)).Times(0);
    EXPECT_CALL(callbacks[1], call(Ref(*backSockets[1])));
    loop.run();
}

TEST_F(MessageLoopTest, testAddPollableTwice)
{
    EXPECT_THROW(loop.addPollable(backSockets[0], [](auto&) {}), std::runtime_error);
}
