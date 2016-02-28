#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <array>
#include <string>

using Bridge::Messaging::MessageHandler;
using Bridge::Messaging::MockMessageHandler;

using testing::ElementsAreArray;
using testing::Return;
using testing::StrictMock;

namespace {
const auto IDENTITY = std::string {"identity"};
const std::array<std::string, 2> PARAMS {{"param1", "param2"}};
}

class MessageHandlerTest : public testing::Test
{
protected:
    StrictMock<MockMessageHandler> messageHandler;
};

TEST_F(MessageHandlerTest, testMessageHandlerSuccess)
{
    EXPECT_CALL(messageHandler, doHandle(IDENTITY, ElementsAreArray(PARAMS)))
        .WillOnce(Return(true));
    EXPECT_TRUE(messageHandler.handle(IDENTITY, PARAMS.begin(), PARAMS.end()));
}

TEST_F(MessageHandlerTest, testMessageHandlerFailure)
{
    EXPECT_CALL(messageHandler, doHandle(IDENTITY, ElementsAreArray(PARAMS)))
        .WillOnce(Return(false));
    EXPECT_FALSE(messageHandler.handle(IDENTITY, PARAMS.begin(), PARAMS.end()));
}
