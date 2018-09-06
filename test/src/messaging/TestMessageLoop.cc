#include "messaging/MessageLoop.hh"
#include "messaging/MessageUtility.hh"
#include "MockMessageLoopCallback.hh"

#include <boost/range/combine.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <array>
#include <csignal>
#include <string>

using Bridge::Messaging::recvMessage;
using Bridge::Messaging::sendMessage;

using testing::_;
using testing::Ref;
using testing::Invoke;

namespace {
using namespace std::string_literals;
constexpr auto N_SOCKETS = 2u;
const auto DEFAULT_MSG = "default"s;
const auto OTHER_MSG = "other"s;
}

class MessageLoopTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (auto&& t : boost::combine(endpoints, frontSockets, backSockets, callbacks)) {
            ON_CALL(t.get<3>(), call(_)).WillByDefault(
                Invoke(
                    [this](auto& socket)
                    {
                        const auto msg = recvMessage<std::string>(socket);
                        EXPECT_EQ(std::make_pair(DEFAULT_MSG, false), msg);
                        std::raise(SIGTERM);
                    }));
            t.get<2>()->bind(t.get<0>());
            t.get<1>().connect(t.get<0>());
            loop.addSocket(
                t.get<2>(),
                [&callback = t.get<3>()](auto& socket)
                {
                    callback.call(socket);
                });
        }
    }

    zmq::context_t context;
    std::array<std::string, N_SOCKETS> endpoints {{
        "inproc://endpoint1",
        "inproc://endpoint2",
    }};
    std::array<zmq::socket_t, N_SOCKETS> frontSockets {{
        {context, zmq::socket_type::dealer},
        {context, zmq::socket_type::dealer},
    }};
    std::array<std::shared_ptr<zmq::socket_t>, N_SOCKETS> backSockets {{
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::dealer),
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::dealer),
    }};
    std::array<
        testing::StrictMock<Bridge::Messaging::MockMessageLoopCallback>,
        N_SOCKETS> callbacks;
    Bridge::Messaging::MessageLoop loop;
};

TEST_F(MessageLoopTest, testSingleMessage)
{
    EXPECT_CALL(callbacks[0], call(Ref(*backSockets[0])));
    sendMessage(frontSockets[0], DEFAULT_MSG);
    loop.run();
}

TEST_F(MessageLoopTest, testMultipleMessages)
{
    EXPECT_CALL(callbacks[0], call(Ref(*backSockets[0])))
        .WillOnce(
            Invoke(
                [this](auto& socket)
                {
                    const auto msg = recvMessage<std::string>(socket);
                    EXPECT_EQ(std::make_pair(OTHER_MSG, false), msg);
                    sendMessage(frontSockets[1], DEFAULT_MSG);
                }));
    EXPECT_CALL(callbacks[1], call(Ref(*backSockets[1])));
    sendMessage(frontSockets[0], OTHER_MSG);
    loop.run();
}

TEST_F(MessageLoopTest, testTerminate)
{
    EXPECT_CALL(callbacks[0], call(Ref(*backSockets[0])))
        .WillOnce(
            Invoke(
                [this](auto& socket)
                {
                    const auto msg = recvMessage<std::string>(socket);
                    EXPECT_EQ(std::make_pair(OTHER_MSG, false), msg);
                    std::raise(SIGTERM);
                }));
    EXPECT_CALL(callbacks[1], call(Ref(*backSockets[1]))).Times(0);
    sendMessage(frontSockets[0], OTHER_MSG);
    loop.run();
}
