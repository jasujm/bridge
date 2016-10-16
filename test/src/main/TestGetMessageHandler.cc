#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "scoring/DuplicateScoreSheet.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "main/Commands.hh"
#include "main/GetMessageHandler.hh"
#include "main/PeerClientControl.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "MockPlayer.hh"
#include "Zip.hh"

#include <gtest/gtest.h>
#include <boost/iterator/indirect_iterator.hpp>

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
using Bridge::MockPlayer;
using Bridge::N_CARDS_PER_PLAYER;
using Bridge::Position;
using Bridge::POSITIONS;
using Bridge::POSITION_TO_STRING_MAP;
using Bridge::Engine::BridgeEngine;
using Bridge::Engine::DuplicateGameManager;
using Bridge::Engine::SimpleCardManager;
using Bridge::Messaging::JsonSerializer;
using Bridge::Messaging::fromJson;
using Bridge::Scoring::DuplicateScoreSheet;

using testing::Return;

using namespace Bridge::Main;
using namespace std::string_literals;

using StringVector = std::vector<std::string>;
using CallVector = std::vector<Call>;
using CardVector = std::vector<CardType>;
using OptionalCardVector = std::vector<boost::optional<Bridge::CardType>>;

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
        peerClientControl->addClient(PLAYER1);
        peerClientControl->addClient(PLAYER2);
        peerClientControl->addClient(PLAYER3);
        peerClientControl->addClient(PLAYER4);
        engine->initiate();
    }

    void shuffle()
    {
        cardManager->shuffle(
            cardTypeIterator(0), cardTypeIterator(Bridge::N_CARDS));
    }

    void makeBidding()
    {
        for (auto&& t : zip(players, CALLS)) {
            engine->call(*t.get<0>(), t.get<1>());
        }
    }

    template<typename... String>
    void request(
        const std::string& identity, String&&... keys)
    {
        const auto keys_ = StringVector {std::forward<String>(keys)...};
        const auto args = {
            KEYS_COMMAND,
            JsonSerializer::serialize(keys_),
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
    std::array<std::shared_ptr<MockPlayer>, 4> players {{
        std::make_shared<MockPlayer>(),
        std::make_shared<MockPlayer>(),
        std::make_shared<MockPlayer>(),
        std::make_shared<MockPlayer>()}};
    std::shared_ptr<BridgeEngine> engine {
        std::make_shared<BridgeEngine>(
            cardManager, gameManager, players.begin(), players.end())};
    std::shared_ptr<PeerClientControl> peerClientControl {
        std::make_shared<PeerClientControl>(
            boost::make_indirect_iterator(players.begin()),
            boost::make_indirect_iterator(players.end()))};
    GetMessageHandler handler {gameManager, engine, peerClientControl};
    std::vector<std::string> reply;
};

TEST_F(GetMessageHandlerTest, testGetFromUnknownClientIsRejected)
{
    std::array<std::string, 0> args;
    EXPECT_FALSE(
        handler.handle(
            PLAYER1, args.begin(), args.end(), std::back_inserter(reply)));
    EXPECT_TRUE(reply.empty());
}

TEST_F(GetMessageHandlerTest, testRequestWithoutKeysIsRejected)
{
    const auto args = {
        KEYS_COMMAND,
        JsonSerializer::serialize(StringVector {ALLOWED_CALLS_COMMAND}),
    };
    EXPECT_FALSE(
        handler.handle(
            "unknown", args.begin(), args.end(), std::back_inserter(reply)));
    EXPECT_TRUE(reply.empty());
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
    for (auto&& t : zip(j, POSITIONS, CALLS)) {
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
    request(PLAYER1, CURRENT_TRICK_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CURRENT_TRICK_COMMAND, reply[0]);
    const auto j = nlohmann::json::parse(reply[1]);
    EXPECT_TRUE(j.empty());
}

TEST_F(GetMessageHandlerTest, testCurrentTrickIfNotEmpty)
{
    shuffle();
    makeBidding();
    const auto& hand = dereference(engine->getHand(*players[1]));
    const auto expected = dereference(hand.getCard(0)).getType();
    engine->play(*players[1], hand, 0);
    request(PLAYER1, CURRENT_TRICK_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CURRENT_TRICK_COMMAND, reply[0]);
    const auto j = nlohmann::json::parse(reply[1]);
    const auto actual = fromJson<CardType>(
        j.at(POSITION_TO_STRING_MAP.left.at(Position::EAST)));
    EXPECT_EQ(expected, actual);
}

TEST_F(GetMessageHandlerTest, testScoreIfEmpty)
{
    request(PLAYER1, SCORE_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(SCORE_COMMAND, reply[0]);
    const auto scores =
        JsonSerializer::deserialize<DuplicateScoreSheet>(reply[1]);
    EXPECT_TRUE(scores.begin() == scores.end());
}

TEST_F(GetMessageHandlerTest, testScoreIfNotEmpty)
{
    const auto PARTNERSHIP = Bridge::Partnership::NORTH_SOUTH;
    const auto SCORE = DuplicateScoreSheet::Score {PARTNERSHIP, 70};
    gameManager->addPassedOut();
    gameManager->addResult(
        PARTNERSHIP,
        {{1, Bridge::Strain::CLUBS}, Bridge::Doubling::UNDOUBLED}, 7);
    request(PLAYER1, SCORE_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(SCORE_COMMAND, reply[0]);
    const auto scores =
        JsonSerializer::deserialize<DuplicateScoreSheet>(reply[1]);
    ASSERT_EQ(2, std::distance(scores.begin(), scores.end()));
    EXPECT_EQ(boost::none, *scores.begin());
    EXPECT_EQ(SCORE, *std::next(scores.begin()));
}
