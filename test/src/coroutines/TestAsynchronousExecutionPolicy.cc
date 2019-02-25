#include "coroutines/AsynchronousExecutionPolicy.hh"
#include "coroutines/CoroutineAdapter.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageHelper.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "MockMessageHandler.hh"
#include "MockPoller.hh"
#include "Blob.hh"

#include <boost/endian/conversion.hpp>
#include <gtest/gtest.h>

#include <array>
#include <string>

using namespace Bridge::Coroutines;
using namespace Bridge::Messaging;
using Bridge::asBytes;
using testing::_;
using testing::Invoke;
using testing::IsEmpty;
using testing::Ref;
using testing::SaveArg;
using testing::WithArgs;

namespace {

using MockAsynchronousMessageHandler =
    MockBasicMessageHandler<AsynchronousExecutionPolicy>;

using namespace Bridge::BlobLiterals;
using namespace std::string_literals;
const auto COMMAND = "command"_B;
const auto MQ_ENDPOINT = "inproc://bridge.test.asyncexecpolicy.mq"s;
const auto CORO_ENDPOINT = "inproc://bridge.test.asyncexecpolicy.coro"s;

}

class AsynchronousExecutionPolicyTest :
    public testing::TestWithParam<StatusCode> {
protected:
    virtual void SetUp()
    {
        messageQueue.addExecutionPolicy(AsynchronousExecutionPolicy {poller});
        messageQueue.trySetHandler(asBytes(COMMAND), handler);
    }

    auto createCoroutine()
    {
        return [this](auto& execution, auto& response)
        {
            execution.await(coroSockets.first);
            auto status = StatusCode {};
            const auto n_recv = coroSockets.first->recv(
                &status, sizeof(status));
            ASSERT_EQ(n_recv, sizeof(status));
            response.setStatus(status);
        };
    }

    zmq::context_t context {};
    MessageQueue messageQueue {};
    testing::NiceMock<MockPoller> poller {};
    std::shared_ptr<MockAsynchronousMessageHandler> handler {
        std::make_shared<MockAsynchronousMessageHandler>()};
    std::pair<CoroutineAdapter::AwaitableSocket, zmq::socket_t> coroSockets {
        ([this]() {
            auto sockets = createSocketPair(context, CORO_ENDPOINT);
            return std::pair {
                std::make_shared<zmq::socket_t>(std::move(sockets.first)),
                std::move(sockets.second)};
        })()};
    std::pair<zmq::socket_t, zmq::socket_t> messageQueueSockets {
        createSocketPair(context, MQ_ENDPOINT)};
};

TEST_P(AsynchronousExecutionPolicyTest, testAsynchronousExecution)
{
    // Invoke coroutine by sending command to message queue socket
    auto socketCallback = Poller::SocketCallback {};
    messageQueueSockets.second.send(COMMAND.data(), COMMAND.size());
    EXPECT_CALL(*handler, doHandle(_, _, IsEmpty(), _)).WillOnce(
        WithArgs<0, 3>(Invoke(createCoroutine())));
    EXPECT_CALL(poller, handleAddPollable(coroSockets.first, _)).WillOnce(
        SaveArg<1>(&socketCallback));
    messageQueue(messageQueueSockets.first);

    // Send status to coroutine communication socket
    const auto status = GetParam();
    coroSockets.second.send(&status, sizeof(status));
    ASSERT_TRUE(socketCallback);
    socketCallback(*coroSockets.first);

    // Check reply
    constexpr auto EXPECTED_N_PARTS = 2;
    auto reply = std::array<zmq::message_t, EXPECTED_N_PARTS> {};
    const auto n_parts = recvMultipart(
        messageQueueSockets.second, reply.begin(), reply.size()).second;
    EXPECT_EQ(EXPECTED_N_PARTS, n_parts);
    EXPECT_EQ(
        status, boost::endian::big_to_native(*reply[0].data<StatusCode>()));
    EXPECT_EQ(asBytes(COMMAND), messageView(reply[1]));
}

INSTANTIATE_TEST_CASE_P(
    StatusCodes, AsynchronousExecutionPolicyTest,
    testing::Values(REPLY_SUCCESS, REPLY_FAILURE));
