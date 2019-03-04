#include "MockMessageHandler.hh"

#include <gtest/gtest.h>

#include <array>
#include <iterator>
#include <vector>

using Bridge::asBytes;
using Bridge::Blob;
using namespace Bridge::Messaging;

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
const auto IDENTITY = Identity { ""s, "identity"_B };
const auto PARAMS = std::array {
    Blob { std::byte {23} },
    Blob { std::byte {34} },
};
const auto OUTPUT1 = "output1"_B;
const auto OUTPUT2 = "output2"_B;
}

class MessageHandlerTest : public testing::Test
{
protected:
    std::vector<Blob> output;
    StrictMock<MockResponse> response;
    StrictMock<MockMessageHandler> messageHandler;
};

TEST_F(MessageHandlerTest, testMessageHandlerSuccess)
{
    EXPECT_CALL(response, handleSetStatus(REPLY_SUCCESS));
    EXPECT_CALL(
        messageHandler,
        doHandle(
            _, IDENTITY, ElementsAre(asBytes(PARAMS[0]), asBytes(PARAMS[1])), _))
        .WillOnce(Respond(REPLY_SUCCESS));;
    messageHandler.handle({}, IDENTITY, PARAMS.begin(), PARAMS.end(), response);
}

TEST_F(MessageHandlerTest, testMessageHandlerFailure)
{
    EXPECT_CALL(response, handleSetStatus(REPLY_FAILURE));
    EXPECT_CALL(
        messageHandler,
        doHandle(
            _, IDENTITY, ElementsAre(asBytes(PARAMS[0]), asBytes(PARAMS[1])), _))
        .WillOnce(Respond(REPLY_FAILURE));
    messageHandler.handle(
        {}, IDENTITY, PARAMS.begin(), PARAMS.end(), response);
}

TEST_F(MessageHandlerTest, testMessageHandlerOutput)
{
    EXPECT_CALL(response, handleSetStatus(REPLY_SUCCESS));
    {
        testing::InSequence guard;
        EXPECT_CALL(response, handleAddFrame(asBytes(OUTPUT1)));
        EXPECT_CALL(response, handleAddFrame(asBytes(OUTPUT2)));
    }
    EXPECT_CALL(messageHandler, doHandle(_, _, _, Ref(response)))
        .WillOnce(Respond(REPLY_SUCCESS, OUTPUT1, OUTPUT2));
    messageHandler.handle(
        {}, IDENTITY, PARAMS.begin(), PARAMS.end(), response);
}
