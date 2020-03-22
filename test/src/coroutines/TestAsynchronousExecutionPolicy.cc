#include "coroutines/AsynchronousExecutionPolicy.hh"
#include "coroutines/CoroutineAdapter.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageHelper.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "MockMessageHandler.hh"
#include "MockCallbackScheduler.hh"
#include "MockPoller.hh"
#include "Blob.hh"

#include <gtest/gtest.h>

#include <array>
#include <string>

using namespace Bridge::Coroutines;
using namespace Bridge::Messaging;
using testing::_;
using testing::Invoke;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Ref;
using testing::SaveArg;
using testing::WithArgs;

namespace {

using MockAsynchronousMessageHandler =
    MockBasicMessageHandler<AsynchronousExecutionPolicy>;

using namespace Bridge::BlobLiterals;
using namespace std::string_view_literals;
const auto TAG = "tag"_BS;
const auto COMMAND = "command"_BS;
constexpr auto MQ_ENDPOINT = "inproc://bridge.test.asyncexecpolicy.mq"sv;
constexpr auto CORO_ENDPOINT = "inproc://bridge.test.asyncexecpolicy.coro"sv;

}

class AsynchronousExecutionPolicyTest :
    public testing::TestWithParam<Bridge::ByteSpan> {
protected:
    virtual void SetUp()
    {
        messageQueue.addExecutionPolicy(
            AsynchronousExecutionPolicy {poller, callbackScheduler});
        messageQueue.trySetHandler(COMMAND, handler);
    }

    auto createCoroutine()
    {
        return [this](auto context, auto& response)
        {
            ensureSocketReadable(context, coroSockets.first);
            auto status_message = Message {};
            recvMessage(*coroSockets.first, status_message);
            response.setStatus(messageView(status_message));
        };
    }

    MessageContext context {};
    MessageQueue messageQueue {};
    std::shared_ptr<NiceMock<MockPoller>> poller {
        std::make_shared<NiceMock<MockPoller>>()};
    std::shared_ptr<NiceMock<MockCallbackScheduler>> callbackScheduler {
        std::shared_ptr<NiceMock<MockCallbackScheduler>>()};
    std::shared_ptr<MockAsynchronousMessageHandler> handler {
        std::make_shared<MockAsynchronousMessageHandler>()};
    std::pair<SharedSocket, Socket> coroSockets {
        ([this]() {
            auto sockets = createSocketPair(context, CORO_ENDPOINT);
            return std::pair {
                makeSharedSocket(std::move(sockets.first)),
                std::move(sockets.second)};
        })()};
    std::pair<Socket, Socket> messageQueueSockets {
        createSocketPair(context, MQ_ENDPOINT)};
};

TEST_P(AsynchronousExecutionPolicyTest, testAsynchronousExecution)
{
    // Invoke coroutine by sending command to message queue socket
    auto socketCallback = Poller::SocketCallback {};
    sendMessage(messageQueueSockets.second, messageBuffer(TAG), true);
    sendMessage(messageQueueSockets.second, messageBuffer(COMMAND));
    EXPECT_CALL(*handler, doHandle(_, _, IsEmpty(), _)).WillOnce(
        WithArgs<0, 3>(Invoke(createCoroutine())));
    EXPECT_CALL(*poller, handleAddPollable(coroSockets.first, _)).WillOnce(
        SaveArg<1>(&socketCallback));
    messageQueue(messageQueueSockets.first);

    // Send status to coroutine communication socket
    const auto status = GetParam();
    sendMessage(coroSockets.second, messageBuffer(status));
    ASSERT_TRUE(socketCallback);
    socketCallback(*coroSockets.first);

    // Check reply
    constexpr auto EXPECTED_N_PARTS = 2;
    auto reply = std::array<Message, EXPECTED_N_PARTS> {};
    const auto n_parts = recvMultipart(
        messageQueueSockets.second, reply.begin(), reply.size()).second;
    EXPECT_EQ(EXPECTED_N_PARTS, n_parts);
    EXPECT_EQ(TAG, messageView(reply[0]));
    EXPECT_EQ(status, messageView(reply[1]));
}

INSTANTIATE_TEST_SUITE_P(
    StatusCodes, AsynchronousExecutionPolicyTest,
    testing::Values(REPLY_SUCCESS, REPLY_FAILURE));
