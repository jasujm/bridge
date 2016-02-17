#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using Bridge::Messaging::MessageHandler;
using Bridge::Messaging::MockMessageHandler;

using testing::ElementsAreArray;
using testing::Return;
using testing::StrictMock;

class MessageHandlerTest : public testing::Test
{
protected:
    std::vector<std::string> params { "param1", "param2" };
    StrictMock<MockMessageHandler> messageHandler;
};

TEST_F(MessageHandlerTest, testMessageHandlerSuccess)
{
    EXPECT_CALL(messageHandler, doHandle(ElementsAreArray(params)))
        .WillOnce(Return(true));
    EXPECT_TRUE(messageHandler.handle(params.begin(), params.end()));
}

TEST_F(MessageHandlerTest, testMessageHandlerFailure)
{
    EXPECT_CALL(messageHandler, doHandle(ElementsAreArray(params)))
        .WillOnce(Return(false));
    EXPECT_FALSE(messageHandler.handle(params.begin(), params.end()));
}
