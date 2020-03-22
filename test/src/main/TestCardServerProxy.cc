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
#include "messaging/MessageQueue.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
#include "messaging/Replies.hh"
#include "messaging/Sockets.hh"
#include "MockCardProtocol.hh"
#include "MockHand.hh"
#include "MockObserver.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
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
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Return;

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;

using ShufflingState = CardManager::ShufflingState;
using ShufflingStateObserver = MockObserver<ShufflingState>;
using CardRevealState = Hand::CardRevealState;

namespace {

const auto CARD_SERVER_ENDPOINT = "inproc://card-server"s;
const auto CARD_SERVER_ENDPOINT2 = "inproc://card-server-2"s;
constexpr auto CONTROL_ENDPOINT = "inproc://control"sv;
const auto PEER = Identity { ""s, "peer"_B };
const auto PEER_POSITIONS = CardProtocol::PositionVector {Positions::SOUTH};
const auto PEER2 = Identity { ""s, "peer2"_B };
const auto PEER2_POSITIONS = CardProtocol::PositionVector {
    Positions::NORTH, Positions::WEST};
const auto SELF_POSITIONS = CardProtocol::PositionVector {Positions::EAST};

MATCHER_P(
    IsSerialized, value,
    std::string {negation ? "is not serialized " : "is serialized "} +
    ::testing::PrintToString(value))
{
    const auto arg_ = JsonSerializer::deserialize<
        std::decay_t<decltype(value)>>(asBytes(arg));
    return value == arg_;
}

template<typename IndexIterator>
auto makeCardIterator(IndexIterator iter)
{
    return boost::make_transform_iterator(iter, enumerateCardType);
}

nlohmann::json makePeerArgsForCardServerProxy(const std::string& endpoint)
{
    return {
        {
            CARD_SERVER_COMMAND,
            {{ ENDPOINT_COMMAND, endpoint }},
        }
    };
}

}

class CardServerProxyTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        bindSocket(proxySocket, CONTROL_ENDPOINT);
        allCards.resize(N_CARDS);
    }

    template<typename CommandMatcher, typename... Matchers>
    void assertCommand(CommandMatcher&& commandMatcher, Matchers&&... matchers)
    {
        recvMessage(proxySocket, routingId);
        auto parts = std::vector<std::string> {};
        auto part = Message {};
        do {
            recvMessage(proxySocket, part);
            parts.emplace_back(part.data<char>(), part.size());
        } while (part.more());
        EXPECT_THAT(
            parts,
            ElementsAre(
                IsEmpty(), commandMatcher, commandMatcher,
                std::forward<Matchers>(matchers)...));
    }

    template<typename Command, typename... Args>
    void reply(Command&& command, Args&&... args)
    {
        auto routing_id = Message {};
        routing_id.copy(routingId);
        sendMessage(proxySocket, std::move(routing_id), true);
        sendEmptyMessage(proxySocket, true);
        sendMessage(proxySocket, messageBuffer(command), true);
        sendMessage(proxySocket, messageBuffer(REPLY_SUCCESS), sizeof...(args) > 0);
        auto command_parts = std::vector<Message> {};
        makeCommandParameters(
            std::back_inserter(command_parts), JsonSerializer {},
            std::forward<Args>(args)...);
        for (auto& part : command_parts) {
            sendMessage(proxySocket, part, &part != &command_parts.back());
        }
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

    MessageContext context;
    Message routingId;
    Socket proxySocket {context, SocketType::router};
    CardServerProxy protocol {context, nullptr, CONTROL_ENDPOINT};
    std::vector<std::optional<CardType>> allCards;
};

TEST_F(CardServerProxyTest, testNoDealMessageHandlers)
{
    EXPECT_FALSE(protocol.getDealMessageHandler());
}

TEST_F(CardServerProxyTest, testAcceptPeerMissingArgs)
{
    EXPECT_FALSE(protocol.acceptPeer(PEER, {Positions::SOUTH}, std::nullopt));
}

TEST_F(CardServerProxyTest, testAcceptPeerInvalidArgs)
{
    const auto args = nlohmann::json::array();
    EXPECT_FALSE(
        protocol.acceptPeer(
            PEER, {Positions::SOUTH}, CardProtocol::OptionalArgs {args}));
}

TEST_F(CardServerProxyTest, testAcceptPeerWithInvalidEndpoint)
{
    const auto args = nlohmann::json {
        {
            CARD_SERVER_COMMAND,
            { ENDPOINT_COMMAND, nullptr }
        }
    };
    EXPECT_FALSE(
        protocol.acceptPeer(
            PEER, {Positions::SOUTH}, CardProtocol::OptionalArgs {args}));
}

TEST_F(CardServerProxyTest, testAcceptPeerMissingPosition)
{
    const auto args = makePeerArgsForCardServerProxy(CARD_SERVER_ENDPOINT);
    EXPECT_FALSE(
        protocol.acceptPeer(PEER, {}, CardProtocol::OptionalArgs {args}));
}

TEST_F(CardServerProxyTest, testCardServerProxy)
{
    EXPECT_TRUE(
        protocol.acceptPeer(
            PEER, {Positions::SOUTH},
            makePeerArgsForCardServerProxy(CARD_SERVER_ENDPOINT)));
    EXPECT_TRUE(
        protocol.acceptPeer(
            PEER2, {Positions::NORTH, Positions::WEST},
            makePeerArgsForCardServerProxy(CARD_SERVER_ENDPOINT2)));
    protocol.initialize();

    assertCommand(
        Eq(INIT_COMMAND), Eq(ORDER_COMMAND), IsSerialized(1), Eq(PEERS_COMMAND),
        IsSerialized(
            std::vector {
                PeerEntry {CARD_SERVER_ENDPOINT2},
                PeerEntry {CARD_SERVER_ENDPOINT} }));

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
    assertCommand(Eq(SHUFFLE_COMMAND));
    assertCommand(
        Eq(REVEAL_COMMAND), Eq(ORDER_COMMAND), IsSerialized(0),
        Eq(CardServer::CARDS_COMMAND),
        IsSerialized(cardsFor(PEER2_POSITIONS.begin(), PEER2_POSITIONS.end())));
    const auto self_card_ns =
        cardsFor(SELF_POSITIONS.begin(), SELF_POSITIONS.end());
    assertCommand(
        Eq(DRAW_COMMAND), Eq(CardServer::CARDS_COMMAND),
        IsSerialized(self_card_ns));
    const auto peer_card_ns =
        cardsFor(PEER_POSITIONS.begin(), PEER_POSITIONS.end());
    assertCommand(
        Eq(REVEAL_COMMAND), Eq(ORDER_COMMAND), IsSerialized(2),
        Eq(CardServer::CARDS_COMMAND), IsSerialized(peer_card_ns));

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
    auto reveal_ns = std::vector<int> {2, 4, 6};
    auto reveal_card_ns = std::vector<int>(
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
    assertCommand(
        Eq(REVEAL_ALL_COMMAND), Eq(CardServer::CARDS_COMMAND),
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
