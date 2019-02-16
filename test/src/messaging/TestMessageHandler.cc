#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <array>
#include <iterator>
#include <vector>

using Bridge::asBytes;
using Bridge::Blob;
using Bridge::Messaging::MessageHandler;
using Bridge::Messaging::MockMessageHandler;

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Invoke;
using testing::Ref;
using testing::Return;
using testing::StrictMock;

namespace {
using namespace Bridge::BlobLiterals;
using namespace std::string_literals;
const auto IDENTITY = Bridge::Messaging::Identity { ""s, "identity"_B };
const auto PARAMS = std::array {
    Blob { std::byte {23} },
    Blob { std::byte {34} },
};
const auto OUTPUTS = std::array {
    Blob { std::byte {123} },
    Blob { std::byte {32} },
};
}

class MessageHandlerTest : public testing::Test
{
protected:
    std::vector<Blob> output;
    Bridge::Messaging::SynchronousExecutionPolicy execution;
    StrictMock<MockMessageHandler> messageHandler;
};

TEST_F(MessageHandlerTest, testMessageHandlerSuccess)
{
    EXPECT_CALL(
        messageHandler,
        doHandle(
            Ref(execution), IDENTITY,
            ElementsAre(asBytes(PARAMS[0]), asBytes(PARAMS[1])), _))
        .WillOnce(Return(true));
    EXPECT_TRUE(
        messageHandler.handle(
            execution, IDENTITY, PARAMS.begin(), PARAMS.end(),
            [](const auto&) {}));
}

TEST_F(MessageHandlerTest, testMessageHandlerFailure)
{
    EXPECT_CALL(
        messageHandler,
        doHandle(
            Ref(execution), IDENTITY,
            ElementsAre(asBytes(PARAMS[0]), asBytes(PARAMS[1])), _))
        .WillOnce(Return(false));
    EXPECT_FALSE(
        messageHandler.handle(
            execution, IDENTITY, PARAMS.begin(), PARAMS.end(),
            [](const auto&) {}));
}

TEST_F(MessageHandlerTest, testMessageHandlerOutput)
{
    EXPECT_CALL(messageHandler, doHandle(Ref(execution), _, _, _))
        .WillOnce(
            Invoke(
                MockMessageHandler::writeToSink(
                    OUTPUTS.begin(), OUTPUTS.end())));
    messageHandler.handle(
        execution, IDENTITY, PARAMS.begin(), PARAMS.end(),
        [this](const auto& b)
        {
            output.emplace_back(b.begin(), b.end());
        });
    EXPECT_THAT(output, ElementsAreArray(OUTPUTS));
}
