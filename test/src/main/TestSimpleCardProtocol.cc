#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "engine/CardManager.hh"
#include "main/CallbackScheduler.hh"
#include "main/Commands.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageHelper.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

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

MATCHER(IsShuffledDeck, "")
{
    const auto cards = JsonSerializer::deserialize<CardVector>(arg);
    return isShuffledDeck(cards.begin(), cards.end());
}

const auto ENDPOINT = "inproc://test"s;
const auto LEADER = Identity { std::byte {123}, std::byte {32} };
const auto PEER = Identity { std::byte {234}, std::byte {43} };

boost::uuids::string_generator STRING_GENERATOR;
const auto GAME_UUID = STRING_GENERATOR("0650f2b2-f9d3-411a-99b2-ddb703065265");

}

class SimpleCardProtocolTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        backSocket.bind(ENDPOINT);
        frontSocket = peerCommandSender->addPeer(context, ENDPOINT);
        for (auto&& handler : protocol.getMessageHandlers()) {
            messageHandlers.emplace(handler);
        }
    }

    bool dealCommand(const Identity& identity, const Uuid& gameUuid = GAME_UUID)
    {
        const auto args = {
            GAME_COMMAND,
            JsonSerializer::serialize(gameUuid),
            CARDS_COMMAND,
            JsonSerializer::serialize(
                CardVector(cardTypeIterator(0), cardTypeIterator(N_CARDS)))};
        auto reply = std::vector<std::string> {};
        const auto success =
            dereference(messageHandlers.at(stringToBlob(DEAL_COMMAND))).handle(
                identity, args.begin(), args.end(),
                [&reply](const auto& b)
                {
                    reply.emplace_back(blobToString(b));
                });
        EXPECT_TRUE(reply.empty());
        return success;
    }

    zmq::context_t context;
    zmq::socket_t backSocket {context, zmq::socket_type::dealer};
    std::shared_ptr<zmq::socket_t> frontSocket;
    std::shared_ptr<CallbackScheduler> callbackScheduler {
        std::make_shared<CallbackScheduler>(context)};
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>(callbackScheduler)};
    SimpleCardProtocol protocol {GAME_UUID, peerCommandSender};
    MessageQueue::HandlerMap messageHandlers;
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

    EXPECT_FALSE(dealCommand(PEER));

    assertCardManagerHasShuffledDeck(*card_manager);

    auto command = std::vector<std::string> {};
    recvAll<std::string>(std::back_inserter(command), backSocket);
    EXPECT_THAT(
        command,
        ElementsAre(
            DEAL_COMMAND, GAME_COMMAND, JsonSerializer::serialize(GAME_UUID),
            CARDS_COMMAND, IsShuffledDeck()));
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

    EXPECT_FALSE(dealCommand(PEER));
    EXPECT_FALSE(dealCommand(LEADER, Bridge::Uuid {}));
    EXPECT_TRUE(dealCommand(LEADER));

    assertCardManagerHasShuffledDeck(*card_manager);
}
