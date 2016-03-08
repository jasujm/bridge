#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <algorithm>
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

const auto IDENTITY = std::string {"identity"};
const std::array<std::string, 2> PARAMS {{"param1", "param2"}};
const std::array<std::string, 2> OUTPUTS {{"output1", "output2"}};

struct DummyMessageHandler : private MessageHandler {
    static bool writeToSink(const std::string&, ParameterRange, OutputSink sink)
    {
        std::for_each(OUTPUTS.begin(), OUTPUTS.end(), sink);
        return true;
    }
};

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
        .WillOnce(Invoke(DummyMessageHandler::writeToSink));
    messageHandler.handle(
        IDENTITY, PARAMS.begin(), PARAMS.end(), std::back_inserter(output));
    EXPECT_THAT(output, ElementsAreArray(OUTPUTS));
}
