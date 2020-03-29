#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "main/Commands.hh"
#include "main/GameStateHelper.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "MockMessageHandler.hh"
#include "MockPlayer.hh"

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

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;

using namespace Bridge::Main;
namespace Positions = Bridge::Positions;

using CallVector = std::vector<Call>;
using CardVector = std::vector<CardType>;
using OptionalCardVector = std::vector<std::optional<Bridge::CardType>>;

boost::uuids::string_generator UUIDGEN;

const auto VALID_GAME = UUIDGEN("884b458d-1e8f-4734-b997-4bb206497d8d");
const auto INVALID_GAME = UUIDGEN("b4c36d82-a19c-488e-9ed7-36095dc90598");

const auto PLAYER1_UUID = UUIDGEN("40cb4cbb-0a67-481b-a7f0-f277424f6811");
const auto PLAYER2_UUID = UUIDGEN("141c9665-04cb-4dea-bbcf-dee749d1e355");
const auto PLAYER3_UUID = UUIDGEN("fd83118a-17e5-41b8-9708-da93fcfb3b2b");
const auto PLAYER4_UUID = UUIDGEN("71e847bc-e677-4ed0-b92a-ad9e72ecad4d");

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

class GameStateHelperTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(*players[0], handleGetUuid()).WillByDefault(Return(PLAYER1_UUID));
        ON_CALL(*players[1], handleGetUuid()).WillByDefault(Return(PLAYER2_UUID));
        ON_CALL(*players[2], handleGetUuid()).WillByDefault(Return(PLAYER3_UUID));
        ON_CALL(*players[3], handleGetUuid()).WillByDefault(Return(PLAYER4_UUID));
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

    auto getStatusValues(
        const Player& player, const std::optional<std::string_view> key)
    {
        auto keys = std::optional<std::vector<std::string>> {};
        if (key) {
            keys = std::vector {std::string {*key}};
        }
        return Bridge::Main::getGameState(player, *engine, *gameManager, keys);
    }

    auto getStatusValue(
        const std::string_view key, const std::string_view subkey,
        const Player* player = nullptr)
    {
        if (!player) {
            player = players[0].get();
        }
        const auto state = getStatusValues(*player, key);
        return state.at(std::string {key}).at(std::string {subkey});
    }

    void assertEmptyStatusValue(
        const std::string_view key, const std::string_view subkey,
        const Player* player = nullptr)
    {
        EXPECT_TRUE(getStatusValue(key, subkey, player).empty()) << key;
    }

    std::shared_ptr<SimpleCardManager> cardManager {
        std::make_shared<SimpleCardManager>()};
    std::shared_ptr<DuplicateGameManager> gameManager {
        std::make_shared<DuplicateGameManager>()};
    std::array<std::shared_ptr<NiceMock<Bridge::MockPlayer>>, 4> players {
        std::make_shared<NiceMock<Bridge::MockPlayer>>(),
        std::make_shared<NiceMock<Bridge::MockPlayer>>(),
        std::make_shared<NiceMock<Bridge::MockPlayer>>(),
        std::make_shared<NiceMock<Bridge::MockPlayer>>(),
    };
    std::shared_ptr<BridgeEngine> engine {
        std::make_shared<BridgeEngine>(cardManager, gameManager)};
};

TEST_F(GameStateHelperTest, testRequestWithoutKeysIncludesAllKeys)
{
    const auto state = getStatusValues(*players[0], std::nullopt);
    constexpr auto ALL_KEYS = std::array {
        PUBSTATE_COMMAND, PRIVSTATE_COMMAND, SELF_COMMAND,
    };
    for (const auto key : ALL_KEYS) {
        EXPECT_TRUE(state.contains(key)) << key;
    }
}

TEST_F(GameStateHelperTest, testPosition)
{
    for (const auto t : boost::combine(Position::all(), players)) {
        const auto expected = t.get<0>();
        const auto actual =
            getStatusValue(SELF_COMMAND, POSITION_COMMAND, t.get<1>().get())
            .get<Position>();
        EXPECT_EQ(expected, actual);
    }
}

TEST_F(GameStateHelperTest, testPositionInTurn)
{
    shuffle();
    const auto position = getStatusValue(
        PUBSTATE_COMMAND, POSITION_IN_TURN_COMMAND)
        .get<Position>();
    EXPECT_EQ(Positions::NORTH, position);
}

TEST_F(GameStateHelperTest, testPositionInTurnBeforeDealStarted)
{
    assertEmptyStatusValue(PUBSTATE_COMMAND, POSITION_IN_TURN_COMMAND);
}

TEST_F(GameStateHelperTest, testAllowedCallsForPlayerInTurn)
{
    shuffle();
    const auto calls = getStatusValue(SELF_COMMAND, ALLOWED_CALLS_COMMAND)
        .get<CallVector>();
    EXPECT_FALSE(calls.empty());
}

TEST_F(GameStateHelperTest, testAllowedCallsForPlayerNotInTurn)
{
    shuffle();
    assertEmptyStatusValue(
        SELF_COMMAND, ALLOWED_CALLS_COMMAND, players[1].get());
}

TEST_F(GameStateHelperTest, testAllowedCallsAfterBidding)
{
    shuffle();
    makeBidding();
    assertEmptyStatusValue(SELF_COMMAND, ALLOWED_CALLS_COMMAND);
}

TEST_F(GameStateHelperTest, testCallsIfEmpty)
{
    shuffle();
    assertEmptyStatusValue(PUBSTATE_COMMAND, CALLS_COMMAND);
}

TEST_F(GameStateHelperTest, testCallsIfNotEmpty)
{
    shuffle();
    makeBidding();
    const auto j = getStatusValue(PUBSTATE_COMMAND, CALLS_COMMAND);
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

TEST_F(GameStateHelperTest, testDeclarerIfBiddingNotCompleted)
{
    assertEmptyStatusValue(PUBSTATE_COMMAND, DECLARER_COMMAND);
}

TEST_F(GameStateHelperTest, testDeclarerIfBiddingCompleted)
{
    shuffle();
    makeBidding();
    const auto position = getStatusValue(PUBSTATE_COMMAND, DECLARER_COMMAND)
        .get<Position>();
    EXPECT_EQ(Positions::NORTH, position);
}

TEST_F(GameStateHelperTest, testContractIfBiddingNotCompleted)
{
    shuffle();
    assertEmptyStatusValue(PUBSTATE_COMMAND, CONTRACT_COMMAND);
}

TEST_F(GameStateHelperTest, testContractIfBiddingCompleted)
{
    shuffle();
    makeBidding();
    const auto contract = getStatusValue(PUBSTATE_COMMAND, CONTRACT_COMMAND)
        .get<Bridge::Contract>();
    EXPECT_EQ(CONTRACT, contract);
}

TEST_F(GameStateHelperTest, testAllowedCardsForPlayerInTurn)
{
    shuffle();
    makeBidding();
    const auto cards = getStatusValue(
        SELF_COMMAND, ALLOWED_CARDS_COMMAND, players[1].get())
        .get<CardVector>();
    EXPECT_TRUE(
        std::equal(
            cardTypeIterator(N_CARDS_PER_PLAYER),
            cardTypeIterator(2 * N_CARDS_PER_PLAYER),
            cards.begin(), cards.end()));
}

TEST_F(GameStateHelperTest, testAllowedCardsForPlayerNotInTurn)
{
    shuffle();
    makeBidding();
    assertEmptyStatusValue(SELF_COMMAND, ALLOWED_CARDS_COMMAND);
}

TEST_F(GameStateHelperTest, testAllowedCardsBeforeBiddingIsCompleted)
{
    shuffle();
    assertEmptyStatusValue(SELF_COMMAND, ALLOWED_CARDS_COMMAND);
}

TEST_F(GameStateHelperTest, testPublicCardsIfEmpty)
{
    assertEmptyStatusValue(PUBSTATE_COMMAND, CARDS_COMMAND);
}

TEST_F(GameStateHelperTest, testPrivateCardsIfEmpty)
{
    assertEmptyStatusValue(PRIVSTATE_COMMAND, CARDS_COMMAND);
}

TEST_F(GameStateHelperTest, testPublicCardsIfNotEmpty)
{
    shuffle();
    const auto j = getStatusValue(PUBSTATE_COMMAND, CARDS_COMMAND);
    for (const auto position : Position::all()) {
        const auto actual = j.at(std::string {position.value()})
            .get<OptionalCardVector>();
        const auto expected =
            OptionalCardVector(N_CARDS_PER_PLAYER, std::nullopt);
        EXPECT_EQ(expected, actual);
    }
}

TEST_F(GameStateHelperTest, testPrivateCardsIfNotEmpty)
{
    shuffle();
    const auto j = getStatusValue(PRIVSTATE_COMMAND, CARDS_COMMAND);
    const auto actual = j.at(std::string {Positions::NORTH.value()})
        .get<OptionalCardVector>();
    const auto expected =
        OptionalCardVector(
            cardTypeIterator(0),
            cardTypeIterator(N_CARDS_PER_PLAYER));
    EXPECT_EQ(expected, actual);
}

TEST_F(GameStateHelperTest, testCurrentTrickIfEmpty)
{
    shuffle();
    assertEmptyStatusValue(PUBSTATE_COMMAND, TRICK_COMMAND);
}

TEST_F(GameStateHelperTest, testCurrentTrickIfNotEmpty)
{
    shuffle();
    makeBidding();
    const auto& hand = dereference(engine->getHand(Positions::EAST));
    const auto expected_card_type =
        dereference(dereference(hand.getCard(0)).getType());
    engine->play(*players[1], hand, 0);
    const auto trick = getStatusValue(PUBSTATE_COMMAND, TRICK_COMMAND);
    ASSERT_EQ(1u, trick.size());
    const auto position_iter = trick[0].find(POSITION_COMMAND);
    ASSERT_NE(trick[0].end(), position_iter);
    EXPECT_EQ(Positions::EAST, position_iter->get<Position>());
    const auto card_iter = trick[0].find(CARD_COMMAND);
    ASSERT_NE(trick[0].end(), card_iter);
    EXPECT_EQ(expected_card_type, card_iter->get<CardType>());
}

TEST_F(GameStateHelperTest, testTricksWon)
{
    const auto tricks_won = getStatusValue(PUBSTATE_COMMAND, TRICKS_WON_COMMAND)
    .get<Bridge::TricksWon>();
    EXPECT_EQ(Bridge::TricksWon(0, 0), tricks_won);
}

TEST_F(GameStateHelperTest, testVulnerability)
{
    const auto vulnerability = getStatusValue(PUBSTATE_COMMAND, VULNERABILITY_COMMAND)
        .get<Bridge::Vulnerability>();
    EXPECT_EQ(Bridge::Vulnerability(false, false), vulnerability);
}
