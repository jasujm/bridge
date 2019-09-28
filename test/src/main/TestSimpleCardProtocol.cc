#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "engine/CardManager.hh"
#include "main/Commands.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/Sockets.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "MockCallbackScheduler.hh"
#include "MockMessageHandler.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

using namespace Bridge;
using namespace Bridge::Main;
using namespace Bridge::Messaging;

using testing::ElementsAre;

using namespace std::string_literals;
using namespace Bridge::BlobLiterals;

using CardVector = std::vector<CardType>;

namespace {

template<typename CardIterator>
bool isShuffledDeck(CardIterator first, CardIterator last)
{
    return std::is_permutation(
        cardTypeIterator(0), cardTypeIterator(N_CARDS),
        first, last);
}

void assertCardManagerHasShuffledDeck(Engine::CardManager& cardManager)
{
    const auto range = to(N_CARDS);
    const auto hand = cardManager.getHand(range.begin(), range.end());
    ASSERT_TRUE(hand);
    auto get_type_func = [](const auto& card) { return card.getType(); };
    EXPECT_TRUE(
        isShuffledDeck(
            boost::make_transform_iterator(hand->begin(), get_type_func),
            boost::make_transform_iterator(hand->end(), get_type_func)));
}

const auto ENDPOINT = "inproc://test"s;
const auto LEADER = Identity { ""s, "leader"_B };
const auto PEER = Identity { ""s, "peer"_B };

boost::uuids::string_generator STRING_GENERATOR;
const auto GAME_UUID = STRING_GENERATOR("0650f2b2-f9d3-411a-99b2-ddb703065265");

}

class SimpleCardProtocolTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket.bind(ENDPOINT);
        frontSocket = peerCommandSender->addPeer(context, ENDPOINT);
    }

    void dealCommand(const Identity& identity, StatusCode expectedStatus)
    {
        const auto args = {
            CARDS_COMMAND,
            JsonSerializer::serialize(
                CardVector(cardTypeIterator(0), cardTypeIterator(N_CARDS)))};
        testing::StrictMock<MockResponse> response;
        EXPECT_CALL(response, handleSetStatus(expectedStatus));
        dereference(dealHandler).handle(
            {}, identity, args.begin(), args.end(), response);
    }

    MessageContext context;
    Socket backSocket {context, SocketType::dealer};
    SharedSocket frontSocket;
    std::shared_ptr<MockCallbackScheduler> callbackScheduler {
        std::make_shared<MockCallbackScheduler>()};
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>(callbackScheduler)};
    SimpleCardProtocol protocol {GAME_UUID, peerCommandSender};
    std::shared_ptr<MessageHandler> dealHandler {
        protocol.getDealMessageHandler()};
};

TEST_F(SimpleCardProtocolTest, testLeader)
{
    EXPECT_TRUE(
        protocol.acceptPeer(
            PEER, {Position::SOUTH, Position::WEST}, std::nullopt));
    protocol.initialize();

    const auto card_manager = protocol.getCardManager();
    ASSERT_TRUE(card_manager);
    card_manager->requestShuffle();

    dealCommand(PEER, REPLY_FAILURE);

    assertCardManagerHasShuffledDeck(*card_manager);

    auto message = Message {};
    recvMessage(backSocket, message);
    EXPECT_EQ(0u, message.size());
    ASSERT_TRUE(message.more());
    recvMessage(backSocket, message);
    EXPECT_EQ(asBytes(DEAL_COMMAND), messageView(message));
    ASSERT_TRUE(message.more());
    recvMessage(backSocket, message);
    EXPECT_EQ(asBytes(GAME_COMMAND), messageView(message));
    ASSERT_TRUE(message.more());
    recvMessage(backSocket, message);
    EXPECT_EQ(
        asBytes(JsonSerializer::serialize(GAME_UUID)), messageView(message));
    ASSERT_TRUE(message.more());
    recvMessage(backSocket, message);
    EXPECT_EQ(asBytes(CARDS_COMMAND), messageView(message));
    ASSERT_TRUE(message.more());
    recvMessage(backSocket, message);
    const auto cards =
        JsonSerializer::deserialize<CardVector>(messageView(message));
    EXPECT_TRUE(isShuffledDeck(cards.begin(), cards.end()));
    EXPECT_FALSE(message.more());
}

TEST_F(SimpleCardProtocolTest, testNotLeader)
{
    EXPECT_TRUE(
        protocol.acceptPeer(
            LEADER, {Position::NORTH, Position::EAST}, std::nullopt));
    EXPECT_TRUE(protocol.acceptPeer(PEER, {Position::SOUTH}, std::nullopt));
    protocol.initialize();

    const auto card_manager = protocol.getCardManager();
    ASSERT_TRUE(card_manager);
    card_manager->requestShuffle();

    dealCommand(PEER, REPLY_FAILURE);
    dealCommand(LEADER, REPLY_SUCCESS);

    assertCardManagerHasShuffledDeck(*card_manager);
}
