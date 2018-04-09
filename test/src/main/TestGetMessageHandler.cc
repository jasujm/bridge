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
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "MockBridgeGameInfo.hh"

#include <boost/range/adaptor/strided.hpp>
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
using Bridge::POSITIONS;
using Bridge::POSITION_TO_STRING_MAP;
using Bridge::Engine::BridgeEngine;
using Bridge::Engine::DuplicateGameManager;
using Bridge::Engine::SimpleCardManager;
using Bridge::Messaging::JsonSerializer;
using Bridge::Messaging::fromJson;

using testing::ReturnPointee;

using namespace Bridge::Main;
using namespace std::string_literals;

using StringVector = std::vector<std::string>;
using CallVector = std::vector<Call>;
using CardVector = std::vector<CardType>;
using OptionalCardVector = std::vector<boost::optional<Bridge::CardType>>;

boost::uuids::string_generator STRING_GENERATOR;
const auto VALID_GAME = STRING_GENERATOR(
    "884b458d-1e8f-4734-b997-4bb206497d8d");
const auto INVALID_GAME = STRING_GENERATOR(
    "b4c36d82-a19c-488e-9ed7-36095dc90598");

const auto PLAYER1 = "player1"s;
const auto PLAYER2 = "player2"s;
const auto PLAYER3 = "player3"s;
const auto PLAYER4 = "player4"s;

std::array<Call, 4> CALLS {{
    Bridge::Bid {1, Bridge::Strain::CLUBS},
    Bridge::Pass {},
    Bridge::Pass {},
    Bridge::Pass {},
}};

const auto CONTRACT = Bridge::Contract {
    { 1, Bridge::Strain::CLUBS },
    { Bridge::Doubling::UNDOUBLED }
};

}

class GetMessageHandlerTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(gameInfo, handleGetEngine())
            .WillByDefault(ReturnPointee(engine));
        ON_CALL(gameInfo, handleGetGameManager())
            .WillByDefault(ReturnPointee(gameManager));
        players[0] = nodePlayerControl->createPlayer(PLAYER1, boost::none);
        players[1] = nodePlayerControl->createPlayer(PLAYER2, boost::none);
        players[2] = nodePlayerControl->createPlayer(PLAYER3, boost::none);
        players[3] = nodePlayerControl->createPlayer(PLAYER4, boost::none);
        for (const auto t : boost::combine(POSITIONS, players)) {
            engine->setPlayer(t.get<0>(), t.get<1>());
        }
        engine->initiate();
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

    template<typename... String>
    void request(
        const std::string& identity, String&&... keys)
    {
        const auto keys_ = StringVector {std::forward<String>(keys)...};
        const auto args = {
            GAME_COMMAND, JsonSerializer::serialize(VALID_GAME),
            KEYS_COMMAND, JsonSerializer::serialize(keys_),
        };
        EXPECT_TRUE(
            handler.handle(
                identity, args.begin(), args.end(), std::back_inserter(reply)));
    }

    void testEmptyRequestReply(
        const std::string& command, const std::string& player = PLAYER1)
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
    testing::NiceMock<MockBridgeGameInfo> gameInfo;
    std::shared_ptr<NodePlayerControl> nodePlayerControl {
        std::make_shared<NodePlayerControl>()};
    GetMessageHandler handler {
        [this](const auto& uuid) -> const BridgeGameInfo*
        {
            if (uuid == VALID_GAME) {
                return &gameInfo;
            }
            return nullptr;
        }, nodePlayerControl};
    std::vector<std::string> reply;
};

TEST_F(GetMessageHandlerTest, testGetFromUnknownClientIsRejected)
{
    const auto args = {
        GAME_COMMAND, JsonSerializer::serialize(VALID_GAME),
        KEYS_COMMAND,
        JsonSerializer::serialize(StringVector {ALLOWED_CALLS_COMMAND}),
    };
    EXPECT_FALSE(
        handler.handle(
            "unknown", args.begin(), args.end(), std::back_inserter(reply)));
    EXPECT_TRUE(reply.empty());
}

TEST_F(GetMessageHandlerTest, testRequestWithoutGameIsRejected)
{
    const auto args = {
        KEYS_COMMAND,
        JsonSerializer::serialize(StringVector {ALLOWED_CALLS_COMMAND}),
    };
    EXPECT_FALSE(
        handler.handle(
            PLAYER1, args.begin(), args.end(), std::back_inserter(reply)));
    EXPECT_TRUE(reply.empty());
}

TEST_F(GetMessageHandlerTest, testRequestWithInvalidGameIsRejected)
{
    const auto args = {
        GAME_COMMAND, JsonSerializer::serialize(INVALID_GAME),
        KEYS_COMMAND,
        JsonSerializer::serialize(StringVector {ALLOWED_CALLS_COMMAND}),
    };
    EXPECT_FALSE(
        handler.handle(
            PLAYER1, args.begin(), args.end(), std::back_inserter(reply)));
    EXPECT_TRUE(reply.empty());
}

TEST_F(GetMessageHandlerTest, testRequestWithoutKeysIncludesAllKeys)
{
    const auto args = {
        GAME_COMMAND, JsonSerializer::serialize(VALID_GAME),
    };
    EXPECT_TRUE(
        handler.handle(
            PLAYER1, args.begin(), args.end(), std::back_inserter(reply)));
    const auto keys = boost::adaptors::stride(reply, 2);
    const auto expected = GetMessageHandler::getAllKeys();
    EXPECT_THAT(keys, testing::UnorderedElementsAreArray(expected));
}

TEST_F(GetMessageHandlerTest, testPositionInTurn)
{
    shuffle();
    request(PLAYER1, POSITION_IN_TURN_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(POSITION_IN_TURN_COMMAND, reply[0]);
    const auto position = JsonSerializer::deserialize<Position>(reply[1]);
    EXPECT_EQ(Position::NORTH, position);
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
    for (auto&& t : boost::combine(j, POSITIONS, CALLS)) {
        const auto position = fromJson<Position>(
            t.get<0>().at(POSITION_COMMAND));
        EXPECT_EQ(t.get<1>(), position);
        const auto call = fromJson<Call>(t.get<0>().at(CALL_COMMAND));
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
    EXPECT_EQ(Position::NORTH, position);
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
    for (const auto position : POSITIONS) {
        const auto actual = fromJson<OptionalCardVector>(
            j.at(POSITION_TO_STRING_MAP.left.at(position)));
        const auto expected = (position == Position::NORTH) ?
            OptionalCardVector(cardTypeIterator(0), cardTypeIterator(N_CARDS_PER_PLAYER)) :
            OptionalCardVector(N_CARDS_PER_PLAYER, boost::none);
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
    const auto& hand = dereference(engine->getHand(Position::EAST));
    const auto expected = std::make_pair(
        Position::EAST,
        dereference(dereference(hand.getCard(0)).getType()));
    engine->play(*players[1], hand, 0);
    request(PLAYER1, TRICK_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(TRICK_COMMAND, reply[0]);
    const auto trick = nlohmann::json::parse(reply[1]);
    ASSERT_EQ(1u, trick.size());
    const auto actual = Bridge::Messaging::jsonToPair<Position, CardType>(
        trick[0], POSITION_COMMAND, CARD_COMMAND);
    EXPECT_EQ(expected, actual);
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
