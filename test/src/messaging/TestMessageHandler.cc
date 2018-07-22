#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <array>
#include <iterator>
#include <string>
#include <vector>

using Bridge::Messaging::MessageHandler;
using Bridge::Messaging::MockMessageHandler;

using testing::_;
using testing::ElementsAreArray;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace {
const auto IDENTITY = Bridge::Messaging::Identity {
    std::byte {123}, std::byte {32} };
const std::array<std::string, 2> PARAMS {{"param1", "param2"}};
const std::array<std::string, 2> OUTPUTS {{"output1", "output2"}};
}

class MessageHandlerTest : public testing::Test
{
protected:
    std::vector<std::string> output;
    StrictMock<MockMessageHandler> messageHandler;
};

TEST_F(MessageHandlerTest, testMessageHandlerSuccess)
{
    EXPECT_CALL(messageHandler, doHandle(IDENTITY, ElementsAreArray(PARAMS), _))
        .WillOnce(Return(true));
    EXPECT_TRUE(
        messageHandler.handle(
            IDENTITY, PARAMS.begin(), PARAMS.end(),
            std::back_inserter(output)));
}

TEST_F(MessageHandlerTest, testMessageHandlerFailure)
{
    EXPECT_CALL(messageHandler, doHandle(IDENTITY, ElementsAreArray(PARAMS), _))
        .WillOnce(Return(false));
    EXPECT_FALSE(
        messageHandler.handle(
            IDENTITY, PARAMS.begin(), PARAMS.end(),
            std::back_inserter(output)));
}

TEST_F(MessageHandlerTest, testMessageHandlerOutput)
{
    EXPECT_CALL(messageHandler, doHandle(_, _, _))
        .WillOnce(
            Invoke(
                MockMessageHandler::writeToSink(
                    OUTPUTS.begin(), OUTPUTS.end())));
    messageHandler.handle(
        IDENTITY, PARAMS.begin(), PARAMS.end(), std::back_inserter(output));
    EXPECT_THAT(output, ElementsAreArray(OUTPUTS));
}
