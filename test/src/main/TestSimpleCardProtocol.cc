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
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
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
const auto LEADER = "leader"s;
const auto PEER = "peer"s;

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

    bool dealCommand(const std::string& identity)
    {
        const auto args = {
            CARDS_COMMAND,
            JsonSerializer::serialize(
                CardVector(cardTypeIterator(0), cardTypeIterator(N_CARDS)))};
        auto reply = std::vector<std::string> {};
        const auto success =
            dereference(messageHandlers.at(DEAL_COMMAND)).handle(
                identity, args.begin(), args.end(), std::back_inserter(reply));
        EXPECT_TRUE(reply.empty());
        return success;
    }

    zmq::context_t context;
    zmq::socket_t backSocket {context, zmq::socket_type::dealer};
    std::shared_ptr<zmq::socket_t> frontSocket;
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>()};
    SimpleCardProtocol protocol {peerCommandSender};
    MessageQueue::HandlerMap messageHandlers;
};

TEST_F(SimpleCardProtocolTest, testLeader)
{
    EXPECT_TRUE(protocol.acceptPeer(PEER, {Position::SOUTH, Position::WEST}));
    protocol.initialize();

    const auto card_manager = protocol.getCardManager();
    ASSERT_TRUE(card_manager);
    card_manager->requestShuffle();

    EXPECT_FALSE(dealCommand(PEER));

    assertCardManagerHasShuffledDeck(*card_manager);

    auto command = std::vector<std::string> {};
    recvAll(std::back_inserter(command), backSocket);
    EXPECT_THAT(
        command, ElementsAre(DEAL_COMMAND, CARDS_COMMAND, IsShuffledDeck()));
}

TEST_F(SimpleCardProtocolTest, testNotLeader)
{
    EXPECT_TRUE(protocol.acceptPeer(LEADER, {Position::NORTH, Position::EAST}));
    EXPECT_TRUE(protocol.acceptPeer(PEER, {Position::SOUTH}));
    protocol.initialize();

    const auto card_manager = protocol.getCardManager();
    ASSERT_TRUE(card_manager);
    card_manager->requestShuffle();

    EXPECT_FALSE(dealCommand(PEER));
    EXPECT_TRUE(dealCommand(LEADER));

    assertCardManagerHasShuffledDeck(*card_manager);
}
