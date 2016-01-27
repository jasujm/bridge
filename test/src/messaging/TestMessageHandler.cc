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
    MessageHandler::ReturnValue returnValue { "returnValue1", "returnValue2" };
    StrictMock<MockMessageHandler> messageHandler;
};

TEST_F(MessageHandlerTest, testMessageHandler)
{
    EXPECT_CALL(messageHandler, handle(ElementsAreArray(params)))
        .WillOnce(Return(returnValue));
    EXPECT_EQ(returnValue, messageHandler(params.begin(), params.end()));
}
