#include "bridge/BridgeConstants.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "cardserver/Commands.hh"
#include "cardserver/PeerEntry.hh"
#include "engine/CardManager.hh"
#include "main/Commands.hh"
#include "main/CardServerProxy.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/Replies.hh"
#include "MockCardProtocol.hh"
#include "MockHand.hh"
#include "MockObserver.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <gtest/gtest.h>
#include <zmq.hpp>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace Bridge;
using namespace Bridge::CardServer;
using namespace Bridge::Engine;
using namespace Bridge::Main;
using namespace Bridge::Messaging;

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Return;

using namespace std::string_literals;

using ShufflingState = CardManager::ShufflingState;
using ShufflingStateObserver = MockObserver<ShufflingState>;
using CardRevealState = Hand::CardRevealState;

namespace {

const auto CARD_SERVER_ENDPOINT = "inproc://card-server"s;
const auto CARD_SERVER_ENDPOINT2 = "inproc://card-server-2"s;
const auto CONTROL_ENDPOINT = "inproc://control"s;
const auto PEER = "peer"s;
const auto PEER_POSITIONS = CardProtocol::PositionVector {Position::SOUTH};
const auto PEER2 = "peer2"s;
const auto PEER2_POSITIONS = CardProtocol::PositionVector {
    Position::NORTH, Position::WEST};
const auto PEER_ENDPOINT = "inproc://control"s;
const auto SELF_POSITIONS = CardProtocol::PositionVector {Position::EAST};

MATCHER_P(
    IsSerialized, value,
    std::string {negation ? "is not serialized " : "is serialized "} +
    ::testing::PrintToString(value))
{
    const auto arg_ = JsonSerializer::deserialize<
        std::decay_t<decltype(value)>>(arg);
    return value == arg_;
}

template<typename IndexIterator>
auto makeCardIterator(IndexIterator iter)
{
    return boost::make_transform_iterator(iter, enumerateCardType);
}

}

class CardServerProxyTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        proxySocket.bind(CONTROL_ENDPOINT);
        protocol.setAcceptor(peerAcceptor);
        for (auto&& handler : protocol.getMessageHandlers()) {
            messageHandlers.emplace(handler);
        }
        allCards.resize(N_CARDS);
    }

    bool peerCommand(
        const std::string& identity,
        const CardProtocol::PositionVector& positions,
        const std::string& cardServerBasePeerEndpoint)
    {
        const auto args = {
            POSITIONS_COMMAND, JsonSerializer::serialize(positions),
            CARD_SERVER_COMMAND,
            JsonSerializer::serialize(cardServerBasePeerEndpoint)};
        auto reply = std::vector<std::string> {};
        const auto success =
            dereference(messageHandlers.at(PEER_COMMAND)).handle(
                identity, args.begin(), args.end(), std::back_inserter(reply));
        EXPECT_TRUE(reply.empty());
        return success;
    }

    template<typename... Matchers>
    void assertMessage(Matchers&&... matchers)
    {
        auto message = std::vector<std::string> {};
        recvAll(std::back_inserter(message), proxySocket);
        EXPECT_THAT(
            message,
            ElementsAre(std::forward<Matchers>(matchers)...));
    }

    template<typename... Args>
    void reply(Args&&... args)
    {
        sendMessage(proxySocket, REPLY_SUCCESS, true);
        sendCommand(
            proxySocket, JsonSerializer {}, std::forward<Args>(args)...);
        const auto sockets = protocol.getSockets();
        const auto iter = sockets.begin();
        ASSERT_NE(iter, sockets.end());
        iter->second(dereference(iter->first));
    }

    template<typename IndexIterator>
    void revealCards(IndexIterator first, IndexIterator last)
    {
        for (; first != last; ++first) {
            const auto n = *first;
            allCards[n] = enumerateCardType(n);
        }
    }

    zmq::context_t context;
    zmq::socket_t proxySocket {context, zmq::socket_type::pair};
    CardServerProxy protocol {context, CONTROL_ENDPOINT};
    std::shared_ptr<MockPeerAcceptor> peerAcceptor {
        std::make_shared<MockPeerAcceptor>()};
    MessageQueue::HandlerMap messageHandlers;
    std::vector<boost::optional<CardType>> allCards;
};

TEST_F(CardServerProxyTest, testRejectPeer)
{
    EXPECT_CALL(*peerAcceptor, acceptPeer(PEER, IsEmpty()))
        .WillOnce(Return(CardProtocol::PeerAcceptState::REJECTED));
    EXPECT_FALSE(peerCommand(PEER, {}, CARD_SERVER_ENDPOINT));
}

TEST_F(CardServerProxyTest, testCardServerProxy)
{
    EXPECT_CALL(*peerAcceptor, acceptPeer(_, _))
        .Times(2)
        .WillOnce(Return(CardProtocol::PeerAcceptState::ACCEPTED))
        .WillOnce(Return(CardProtocol::PeerAcceptState::ALL_ACCEPTED));
    EXPECT_TRUE(
        peerCommand(PEER, {Position::SOUTH}, CARD_SERVER_ENDPOINT));
    EXPECT_TRUE(
        peerCommand(
            PEER2, {Position::NORTH, Position::WEST}, CARD_SERVER_ENDPOINT2));

    assertMessage(
        INIT_COMMAND, ORDER_COMMAND, IsSerialized(1), PEERS_COMMAND,
        IsSerialized(
            std::vector<PeerEntry> {
                PeerEntry {PEER2, CARD_SERVER_ENDPOINT2},
                PeerEntry {PEER, boost::none} }));

    const auto manager = protocol.getCardManager();
    ASSERT_TRUE(manager);
    EXPECT_FALSE(manager->isShuffleCompleted());
    {
        const auto observer = std::make_shared<ShufflingStateObserver>();
        EXPECT_CALL(*observer, handleNotify(ShufflingState::REQUESTED));
        manager->subscribe(observer);
        manager->requestShuffle();
    }
    EXPECT_FALSE(manager->isShuffleCompleted());
    assertMessage(SHUFFLE_COMMAND);
    assertMessage(
        REVEAL_COMMAND, ID_COMMAND, IsSerialized(PEER2),
        CardServer::CARDS_COMMAND,
        IsSerialized(cardsFor(PEER2_POSITIONS.begin(), PEER2_POSITIONS.end())));
    const auto self_card_ns =
        cardsFor(SELF_POSITIONS.begin(), SELF_POSITIONS.end());
    assertMessage(
        DRAW_COMMAND, CardServer::CARDS_COMMAND, IsSerialized(self_card_ns));
    const auto peer_card_ns =
        cardsFor(PEER_POSITIONS.begin(), PEER_POSITIONS.end());
    assertMessage(
        REVEAL_COMMAND, ID_COMMAND, IsSerialized(PEER),
        CardServer::CARDS_COMMAND, IsSerialized(peer_card_ns));

    revealCards(self_card_ns.begin(), self_card_ns.end());
    reply(SHUFFLE_COMMAND);
    reply(REVEAL_COMMAND);
    reply(DRAW_COMMAND, std::make_pair(CardServer::CARDS_COMMAND, allCards));
    {
        const auto observer = std::make_shared<ShufflingStateObserver>();
        EXPECT_CALL(*observer, handleNotify(ShufflingState::COMPLETED))
            .WillOnce(
                InvokeWithoutArgs(
                    [&manager = *manager]()
                    {
                        EXPECT_TRUE(manager.isShuffleCompleted());
                    }));
        manager->subscribe(observer);
        reply(REVEAL_COMMAND);
    }

    const auto self_hand = manager->getHand(
        self_card_ns.begin(), self_card_ns.end());
    ASSERT_TRUE(self_hand);
    EXPECT_TRUE(
        std::equal(
            makeCardIterator(self_card_ns.begin()),
            makeCardIterator(self_card_ns.end()),
            self_hand->begin(), self_hand->end(),
            [](const auto& cardType, const auto& card)
            {
                return cardType == card.getType();
            }));

    const auto peer_hand = manager->getHand(
        peer_card_ns.begin(), peer_card_ns.end());
    ASSERT_TRUE(peer_hand);
    EXPECT_TRUE(
        std::none_of(
            peer_hand->begin(), peer_hand->end(),
            [](const auto& card) { return card.isKnown(); }));
    auto reveal_ns = std::vector<std::size_t> {2, 4, 6};
    auto reveal_card_ns = std::vector<std::size_t>(
        containerAccessIterator(reveal_ns.begin(), peer_card_ns),
        containerAccessIterator(reveal_ns.end(), peer_card_ns));
    {
        const auto observer =
            std::make_shared<MockCardRevealStateObserver>();
        EXPECT_CALL(
            *observer,
            handleNotify(
                CardRevealState::REQUESTED, ElementsAreArray(reveal_ns)));
        peer_hand->subscribe(observer);
        peer_hand->requestReveal(reveal_ns.begin(), reveal_ns.end());
    }
    assertMessage(
        REVEAL_ALL_COMMAND, CardServer::CARDS_COMMAND,
        IsSerialized(reveal_card_ns));
    revealCards(reveal_card_ns.begin(), reveal_card_ns.end());
    {
        const auto observer =
            std::make_shared<MockCardRevealStateObserver>();
        EXPECT_CALL(
            *observer,
            handleNotify(
                CardRevealState::COMPLETED, ElementsAreArray(reveal_ns)));
        peer_hand->subscribe(observer);
        reply(
            REVEAL_ALL_COMMAND,
            std::make_pair(CardServer::CARDS_COMMAND, allCards));
    }
    EXPECT_TRUE(
        std::equal(
            makeCardIterator(reveal_card_ns.begin()),
            makeCardIterator(reveal_card_ns.end()),
            reveal_ns.begin(), reveal_ns.end(),
            [&peer_hand](const auto& cardType, const auto n)
            {
                return cardType == dereference(peer_hand->getCard(n)).getType();
            }));

    manager->requestShuffle();
    EXPECT_FALSE(manager->isShuffleCompleted());
}
