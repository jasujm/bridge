#include "main/SimpleCardProtocol.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Hand.hh"
#include "engine/CardManager.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>

using Bridge::cardTypeIterator;
using Bridge::N_CARDS;
using Bridge::Main::SimpleCardProtocol;
using Bridge::Messaging::JsonSerializer;
using testing::ElementsAre;
using testing::IsNull;
using testing::NotNull;
using testing::Pointee;
using testing::Return;

using namespace std::string_literals;

using CardVector = std::vector<Bridge::CardType>;

namespace {

class MockFunctions {
public:
    MOCK_METHOD1(isLeader, bool(const std::string*));
};

template<typename CardIterator>
bool isShuffledDeck(CardIterator first, CardIterator last)
{
    return std::is_permutation(
        cardTypeIterator(0), cardTypeIterator(N_CARDS),
        first, last);
}

void assertCardManagerHasShuffledDeck(Bridge::Engine::CardManager& cardManager)
{
    const auto range = Bridge::to(N_CARDS);
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

}

class SimpleCardProtocolTest : public testing::Test {
protected:
    zmq::context_t context;
    testing::NiceMock<MockFunctions> functions;
    std::shared_ptr<Bridge::Main::PeerCommandSender> peerCommandSender {
        std::make_shared<Bridge::Main::PeerCommandSender>()};
    SimpleCardProtocol protocol {
        [this](const auto arg) { return functions.isLeader(arg); },
        peerCommandSender};
};

TEST_F(SimpleCardProtocolTest, testLeader)
{
    ON_CALL(functions, isLeader(IsNull())).WillByDefault(Return(true));
    ON_CALL(functions, isLeader(NotNull())).WillByDefault(Return(false));

    const auto endpoint = "inproc://test"s;
    zmq::socket_t backSocket {context, zmq::socket_type::dealer};
    backSocket.bind(endpoint);
    const auto frontSocket = peerCommandSender->addPeer(context, endpoint);

    const auto card_manager = protocol.getCardManager();
    ASSERT_TRUE(card_manager);
    card_manager->requestShuffle();
    assertCardManagerHasShuffledDeck(*card_manager);

    auto command = std::vector<std::string> {};
    Bridge::Messaging::recvAll(std::back_inserter(command), backSocket);
    EXPECT_THAT(
        command,
        ElementsAre(
            SimpleCardProtocol::DEAL_COMMAND,
            IsShuffledDeck()));
}

TEST_F(SimpleCardProtocolTest, testNotLeader)
{
    const auto leader = "leader"s;
    ON_CALL(functions, isLeader(Pointee(leader))).WillByDefault(Return(true));
    ON_CALL(functions, isLeader(IsNull())).WillByDefault(Return(false));

    const auto card_manager = protocol.getCardManager();
    ASSERT_TRUE(card_manager);
    card_manager->requestShuffle();

    const auto message_handlers = protocol.getMessageHandlers();
    const auto handler_map = Bridge::Messaging::MessageQueue::HandlerMap(
        message_handlers.begin(), message_handlers.end());
    auto& handler = Bridge::dereference(
        handler_map.at(SimpleCardProtocol::DEAL_COMMAND));
    const auto card_vector = CardVector(
        cardTypeIterator(0), cardTypeIterator(N_CARDS));
    std::array<std::string, 1> params {{
        JsonSerializer::serialize(card_vector)
    }};
    std::vector<std::string> output;
    EXPECT_TRUE(
        handler.handle(
            leader, params.begin(), params.end(), std::back_inserter(output)));
    EXPECT_TRUE(output.empty());
    assertCardManagerHasShuffledDeck(*card_manager);
}
