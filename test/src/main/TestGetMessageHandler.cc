#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Player.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "main/Commands.hh"
#include "main/GetMessageHandler.hh"
#include "main/NodePlayerControl.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "MockBridgeGameInfo.hh"
#include "MockMessageHandler.hh"

#include <boost/range/adaptors.hpp>
#include <boost/range/combine.hpp>
#include <boost/uuid/string_generator.hpp>
#include <gtest/gtest.h>

#include <array>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bridge::Call;
using Bridge::CardType;
using Bridge::cardTypeIterator;
using Bridge::N_CARDS_PER_PLAYER;
using Bridge::Player;
using Bridge::Position;
using Bridge::Engine::BridgeEngine;
using Bridge::Engine::DuplicateGameManager;
using Bridge::Engine::SimpleCardManager;
using Bridge::Messaging::Identity;
using Bridge::Messaging::JsonSerializer;
using Bridge::Messaging::REPLY_FAILURE;
using Bridge::Messaging::REPLY_SUCCESS;

using testing::_;
using testing::NiceMock;
using testing::ReturnPointee;
using testing::NiceMock;

using namespace Bridge::BlobLiterals;
using namespace Bridge::Main;
namespace Positions = Bridge::Positions;

using CallVector = std::vector<Call>;
using CardVector = std::vector<CardType>;
using OptionalCardVector = std::vector<std::optional<Bridge::CardType>>;

using namespace std::string_literals;

boost::uuids::string_generator STRING_GENERATOR;
const auto VALID_GAME = STRING_GENERATOR(
    "884b458d-1e8f-4734-b997-4bb206497d8d");
const auto INVALID_GAME = STRING_GENERATOR(
    "b4c36d82-a19c-488e-9ed7-36095dc90598");

const auto PLAYER1 = Identity { ""s, "player1"_B };
const auto PLAYER2 = Identity { ""s, "player2"_B };
const auto PLAYER3 = Identity { ""s, "player3"_B };
const auto PLAYER4 = Identity { ""s, "player4"_B };

constexpr auto CALLS = std::array<Call, 4> {
    Bridge::Bid {1, Bridge::Strains::CLUBS},
    Bridge::Pass {},
    Bridge::Pass {},
    Bridge::Pass {},
};

const auto CONTRACT = Bridge::Contract {
    { 1, Bridge::Strains::CLUBS },
    { Bridge::Doublings::UNDOUBLED }
};

}

class GetMessageHandlerTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(*gameInfo, handleGetEngine())
            .WillByDefault(ReturnPointee(engine));
        ON_CALL(*gameInfo, handleGetGameManager())
            .WillByDefault(ReturnPointee(gameManager));
        players[0] = nodePlayerControl->createPlayer(PLAYER1, std::nullopt);
        players[1] = nodePlayerControl->createPlayer(PLAYER2, std::nullopt);
        players[2] = nodePlayerControl->createPlayer(PLAYER3, std::nullopt);
        players[3] = nodePlayerControl->createPlayer(PLAYER4, std::nullopt);
        for (const auto t : boost::combine(Position::all(), players)) {
            engine->setPlayer(t.get<0>(), t.get<1>());
        }
        engine->startDeal();
    }

    void shuffle()
    {
        cardManager->shuffle(
            cardTypeIterator(0), cardTypeIterator(Bridge::N_CARDS));
    }

    void makeBidding()
    {
        for (auto&& t : boost::combine(players, CALLS)) {
            engine->call(*t.get<0>(), t.get<1>());
        }
    }

    void request(
        const Identity& identity, const std::optional<std::string_view> key)
    {
        auto args = std::vector<std::string> {};
        if (key) {
            args.emplace_back(KEYS_COMMAND);
            args.emplace_back(JsonSerializer::serialize(std::vector {*key}));
        }
        EXPECT_CALL(response, handleSetStatus(REPLY_SUCCESS));
        EXPECT_CALL(response, handleAddFrame(_))
            .WillRepeatedly(
                testing::Invoke(
                    [this](auto&& frame)
                    {
                        reply.emplace_back(blobToString(frame));
                    }));
        handler.handle({}, identity, args.begin(), args.end(), response);
    }

    void testEmptyRequestReply(
        const std::string_view command, const Identity& player = PLAYER1)
    {
        request(player, command);
        ASSERT_EQ(2u, reply.size());
        EXPECT_EQ(command, reply[0]);
        const auto j = nlohmann::json::parse(reply[1]);
        EXPECT_TRUE(j.empty());
    }

    std::shared_ptr<SimpleCardManager> cardManager {
        std::make_shared<SimpleCardManager>()};
    std::shared_ptr<DuplicateGameManager> gameManager {
        std::make_shared<DuplicateGameManager>()};
    std::array<std::shared_ptr<Player>, 4> players;
    std::shared_ptr<BridgeEngine> engine {
        std::make_shared<BridgeEngine>(cardManager, gameManager)};
    std::shared_ptr<NiceMock<MockBridgeGameInfo>> gameInfo {
        std::make_shared<NiceMock<MockBridgeGameInfo>>()};
    std::shared_ptr<NodePlayerControl> nodePlayerControl {
        std::make_shared<NodePlayerControl>()};
    testing::StrictMock<Bridge::Messaging::MockResponse> response;
    GetMessageHandler handler {gameInfo, nodePlayerControl};
    std::vector<std::string> reply;
};

TEST_F(GetMessageHandlerTest, testGetFromUnknownClientIsRejected)
{
    const auto args = {
        std::string {KEYS_COMMAND},
        JsonSerializer::serialize(std::vector {ALLOWED_CALLS_COMMAND}),
    };
    EXPECT_CALL(response, handleSetStatus(REPLY_FAILURE));
    handler.handle({}, {}, args.begin(), args.end(), response);
}

TEST_F(GetMessageHandlerTest, testRequestWithoutKeysIncludesAllKeys)
{
    request(PLAYER1, std::nullopt);
    const auto keys_in_reply = reply |
        boost::adaptors::strided(2) |
        boost::adaptors::transformed(
            [](const auto& key) { return std::string_view {key}; });
    constexpr auto ALL_KEYS = std::array {
        POSITION_COMMAND, POSITION_IN_TURN_COMMAND, ALLOWED_CALLS_COMMAND,
        CALLS_COMMAND, DECLARER_COMMAND, CONTRACT_COMMAND, ALLOWED_CARDS_COMMAND,
        CARDS_COMMAND, TRICK_COMMAND, TRICKS_WON_COMMAND, VULNERABILITY_COMMAND
    };
    EXPECT_THAT(keys_in_reply, testing::UnorderedElementsAreArray(ALL_KEYS));
}

TEST_F(GetMessageHandlerTest, testPositionInTurn)
{
    shuffle();
    request(PLAYER1, POSITION_IN_TURN_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(POSITION_IN_TURN_COMMAND, reply[0]);
    const auto position = JsonSerializer::deserialize<Position>(reply[1]);
    EXPECT_EQ(Positions::NORTH, position);
}

TEST_F(GetMessageHandlerTest, testPositionInTurnBeforeDealStarted)
{
    testEmptyRequestReply(POSITION_IN_TURN_COMMAND);
}

TEST_F(GetMessageHandlerTest, testAllowedCallsForPlayerInTurn)
{
    shuffle();
    request(PLAYER1, ALLOWED_CALLS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(ALLOWED_CALLS_COMMAND, reply[0]);
    const auto calls = JsonSerializer::deserialize<CallVector>(reply[1]);
    EXPECT_FALSE(calls.empty());
}

TEST_F(GetMessageHandlerTest, testAllowedCallsForPlayerNotInTurn)
{
    shuffle();
    testEmptyRequestReply(ALLOWED_CALLS_COMMAND, PLAYER2);
}

TEST_F(GetMessageHandlerTest, testAllowedCallsAfterBidding)
{
    shuffle();
    makeBidding();
    testEmptyRequestReply(ALLOWED_CALLS_COMMAND);
}

TEST_F(GetMessageHandlerTest, testCallsIfEmpty)
{
    shuffle();
    testEmptyRequestReply(CALLS_COMMAND);
}

TEST_F(GetMessageHandlerTest, testCallsIfNotEmpty)
{
    shuffle();
    makeBidding();
    request(PLAYER1, CALLS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CALLS_COMMAND, reply[0]);
    const auto j = nlohmann::json::parse(reply[1]);
    for (auto&& t : boost::combine(j, Position::all(), CALLS)) {
        const auto position_iter = t.get<0>().find(POSITION_COMMAND);
        ASSERT_NE(t.get<0>().end(), position_iter);
        const auto position = position_iter->get<Position>();
        EXPECT_EQ(t.get<1>(), position);
        const auto call_iter = t.get<0>().find(CALL_COMMAND);
        ASSERT_NE(t.get<0>().end(), call_iter);
        const auto call = call_iter->get<Call>();
        EXPECT_EQ(t.get<2>(), call);
    }
}

TEST_F(GetMessageHandlerTest, testDeclarerIfBiddingNotCompleted)
{
    testEmptyRequestReply(DECLARER_COMMAND);
}

TEST_F(GetMessageHandlerTest, testDeclarerIfBiddingCompleted)
{
    shuffle();
    makeBidding();
    request(PLAYER1, DECLARER_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(DECLARER_COMMAND, reply[0]);
    const auto position = JsonSerializer::deserialize<Position>(reply[1]);
    EXPECT_EQ(Positions::NORTH, position);
}

TEST_F(GetMessageHandlerTest, testContractIfBiddingNotCompleted)
{
    shuffle();
    testEmptyRequestReply(CONTRACT_COMMAND);
}

TEST_F(GetMessageHandlerTest, testContractIfBiddingCompleted)
{
    shuffle();
    makeBidding();
    request(PLAYER1, CONTRACT_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CONTRACT_COMMAND, reply[0]);
    const auto contract = JsonSerializer::deserialize<Bridge::Contract>(
        reply[1]);
    EXPECT_EQ(CONTRACT, contract);
}

TEST_F(GetMessageHandlerTest, testAllowedCardsForPlayerInTurn)
{
    shuffle();
    makeBidding();
    request(PLAYER2, ALLOWED_CARDS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(ALLOWED_CARDS_COMMAND, reply[0]);
    const auto cards = JsonSerializer::deserialize<CardVector>(reply[1]);
    EXPECT_TRUE(
        std::equal(
            cardTypeIterator(N_CARDS_PER_PLAYER),
            cardTypeIterator(2 * N_CARDS_PER_PLAYER),
            cards.begin(), cards.end()));
}

TEST_F(GetMessageHandlerTest, testAllowedCardsForPlayerNotInTurn)
{
    shuffle();
    makeBidding();
    testEmptyRequestReply(ALLOWED_CARDS_COMMAND);
}

TEST_F(GetMessageHandlerTest, testAllowedCardsBeforeBiddingIsCompleted)
{
    shuffle();
    testEmptyRequestReply(ALLOWED_CARDS_COMMAND);
}

TEST_F(GetMessageHandlerTest, testCardsIfEmpty)
{
    testEmptyRequestReply(CARDS_COMMAND);
}

TEST_F(GetMessageHandlerTest, testCardsIfNotEmpty)
{
    shuffle();
    request(PLAYER1, CARDS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CARDS_COMMAND, reply[0]);
    const auto j = nlohmann::json::parse(reply[1]);
    for (const auto position : Position::all()) {
        const auto actual = j.at(std::string {position.value()}).get<OptionalCardVector>();
        const auto expected = (position == Positions::NORTH) ?
            OptionalCardVector(cardTypeIterator(0), cardTypeIterator(N_CARDS_PER_PLAYER)) :
            OptionalCardVector(N_CARDS_PER_PLAYER, std::nullopt);
        EXPECT_EQ(expected, actual);
    }
}

TEST_F(GetMessageHandlerTest, testCurrentTrickIfEmpty)
{
    shuffle();
    testEmptyRequestReply(TRICK_COMMAND);
}

TEST_F(GetMessageHandlerTest, testCurrentTrickIfNotEmpty)
{
    shuffle();
    makeBidding();
    const auto& hand = dereference(engine->getHand(Positions::EAST));
    const auto expected_card_type =
        dereference(dereference(hand.getCard(0)).getType());
    engine->play(*players[1], hand, 0);
    request(PLAYER1, TRICK_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(TRICK_COMMAND, reply[0]);
    const auto trick = nlohmann::json::parse(reply[1]);
    ASSERT_EQ(1u, trick.size());
    const auto position_iter = trick[0].find(POSITION_COMMAND);
    ASSERT_NE(trick[0].end(), position_iter);
    EXPECT_EQ(Positions::EAST, position_iter->get<Position>());
    const auto card_iter = trick[0].find(CARD_COMMAND);
    ASSERT_NE(trick[0].end(), card_iter);
    EXPECT_EQ(expected_card_type, card_iter->get<CardType>());
}

TEST_F(GetMessageHandlerTest, testTricksWon)
{
    request(PLAYER1, TRICKS_WON_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(TRICKS_WON_COMMAND, reply[0]);
    EXPECT_EQ(
        Bridge::TricksWon(0, 0),
        JsonSerializer::deserialize<Bridge::TricksWon>(reply[1]));
}

TEST_F(GetMessageHandlerTest, testVulnerability)
{
    request(PLAYER1, VULNERABILITY_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(VULNERABILITY_COMMAND, reply[0]);
    EXPECT_EQ(
        Bridge::Vulnerability(false, false),
        JsonSerializer::deserialize<Bridge::Vulnerability>(reply[1]));
}
