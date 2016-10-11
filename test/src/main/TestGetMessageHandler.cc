#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
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
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "MockPlayer.hh"

#include <gtest/gtest.h>
#include <boost/iterator/indirect_iterator.hpp>

#include <array>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bridge::cardTypeIterator;
using Bridge::MockPlayer;
using Bridge::N_CARDS_PER_PLAYER;
using Bridge::Engine::BridgeEngine;
using Bridge::Engine::DuplicateGameManager;
using Bridge::Engine::SimpleCardManager;
using Bridge::Messaging::JsonSerializer;
using Bridge::Scoring::DuplicateScoreSheet;

using testing::Return;

using namespace Bridge::Main;
using namespace std::string_literals;

using StringVector = std::vector<std::string>;
using CallVector = std::vector<Bridge::Call>;
using CardVector = std::vector<Bridge::CardType>;
using OptionalCardVector = std::vector<boost::optional<Bridge::CardType>>;

const auto PLAYER1 = "player1"s;
const auto PLAYER2 = "player2"s;
const auto PLAYER3 = "player3"s;
const auto PLAYER4 = "player4"s;

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
        engine->call(*players[0], Bridge::Bid {1, Bridge::Strain::CLUBS});
        engine->call(*players[1], Bridge::Pass {});
        engine->call(*players[2], Bridge::Pass {});
        engine->call(*players[3], Bridge::Pass {});
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
    request(PLAYER2, ALLOWED_CALLS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(ALLOWED_CALLS_COMMAND, reply[0]);
    const auto calls = JsonSerializer::deserialize<CallVector>(reply[1]);
    EXPECT_TRUE(calls.empty());
}

TEST_F(GetMessageHandlerTest, testAllowedCallsAfterBidding)
{
    shuffle();
    makeBidding();
    request(PLAYER1, ALLOWED_CALLS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(ALLOWED_CALLS_COMMAND, reply[0]);
    const auto calls = JsonSerializer::deserialize<CallVector>(reply[1]);
    EXPECT_TRUE(calls.empty());
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
    request(PLAYER1, ALLOWED_CARDS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(ALLOWED_CARDS_COMMAND, reply[0]);
    const auto cards = JsonSerializer::deserialize<CardVector>(reply[1]);
    EXPECT_TRUE(cards.empty());
}

TEST_F(GetMessageHandlerTest, testAllowedCardsBeforeBiddingIsCompleted)
{
    shuffle();
    request(PLAYER1, ALLOWED_CARDS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(ALLOWED_CARDS_COMMAND, reply[0]);
    const auto cards = JsonSerializer::deserialize<CardVector>(reply[1]);
    EXPECT_TRUE(cards.empty());
}

TEST_F(GetMessageHandlerTest, testCardsIfEmpty)
{
    request(PLAYER1, CARDS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CARDS_COMMAND, reply[0]);
    const auto j = nlohmann::json::parse(reply[1]);
    EXPECT_EQ(0u, j.size());
}

TEST_F(GetMessageHandlerTest, testCardsIfNotEmpty)
{
    shuffle();
    request(PLAYER1, CARDS_COMMAND);
    ASSERT_EQ(2u, reply.size());
    EXPECT_EQ(CARDS_COMMAND, reply[0]);
    const auto j = nlohmann::json::parse(reply[1]);
    for (const auto position : Bridge::POSITIONS) {
        const auto actual = Bridge::Messaging::fromJson<OptionalCardVector>(
            j.at(Bridge::POSITION_TO_STRING_MAP.left.at(position)));
        const auto expected = (position == Bridge::Position::NORTH) ?
            OptionalCardVector(cardTypeIterator(0), cardTypeIterator(N_CARDS_PER_PLAYER)) :
            OptionalCardVector(N_CARDS_PER_PLAYER, boost::none);
        EXPECT_EQ(expected, actual);
    }
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
