// FIXME: These unit tests should be rewritten in terms of the new
// Deal interface instead of tediously building the deal state using
// bridge engine

#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
#include "bridge/Deal.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
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
#include "messaging/UuidJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "MockMessageHandler.hh"
#include "MockPlayer.hh"
#include "Utility.hh"

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
        cardManager->shuffle(
            cardTypeIterator(0), cardTypeIterator(Bridge::N_CARDS));
    }

    void makeBidding()
    {
        for (auto&& t : boost::combine(players, CALLS)) {
            engine->call(*t.get<0>(), t.get<1>());
        }
    }

    auto getPubstateValue(const std::string_view key)
    {
        auto ret = GameState {};
        Bridge::Main::emplacePubstate(engine->getCurrentDeal(), ret);
        return ret.at(std::string {PUBSTATE_COMMAND}).at(std::string {key});
    }

    auto getPrivstateValue(const Player& player, const std::string_view key)
    {
        auto ret = GameState {};
        Bridge::Main::emplacePrivstate(
            engine->getCurrentDeal(), engine->getPosition(player), ret);
        return ret.at(std::string {PRIVSTATE_COMMAND}).at(std::string {key});
    }

    auto getSelfValue(const Player& player, const std::string_view key)
    {
        auto ret = GameState {};
        Bridge::Main::emplaceSelf(
            engine->getCurrentDeal(), engine->getPosition(player), ret);
        return ret.at(std::string {SELF_COMMAND}).at(std::string {key});
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

TEST_F(GameStateHelperTest, testDealUuid)
{
    EXPECT_EQ(
        dereference(engine->getCurrentDeal()).getUuid(),
        getPubstateValue(DEAL_COMMAND).get<Bridge::Uuid>());
}

TEST_F(GameStateHelperTest, testPosition)
{
    for (const auto t : boost::combine(Position::all(), players)) {
        const auto expected = t.get<0>();
        const auto actual = getSelfValue(*t.get<1>(), POSITION_COMMAND).get<Position>();
        EXPECT_EQ(expected, actual);
    }
}

TEST_F(GameStateHelperTest, testPositionInTurn)
{
    const auto position = getPubstateValue(POSITION_IN_TURN_COMMAND).get<Position>();
    EXPECT_EQ(Positions::NORTH, position);
}

TEST_F(GameStateHelperTest, testAllowedCallsForPlayerInTurn)
{
    const auto calls = getSelfValue(*players[0], ALLOWED_CALLS_COMMAND);
    EXPECT_FALSE(calls.empty());
}

TEST_F(GameStateHelperTest, testAllowedCallsForPlayerNotInTurn)
{
    const auto calls = getSelfValue(*players[1], ALLOWED_CALLS_COMMAND);
    EXPECT_TRUE(calls.empty());
}

TEST_F(GameStateHelperTest, testAllowedCallsAfterBidding)
{
    makeBidding();
    const auto calls = getSelfValue(*players[0], ALLOWED_CALLS_COMMAND);
    EXPECT_TRUE(calls.empty());
}

TEST_F(GameStateHelperTest, testCallsIfEmpty)
{
    const auto calls = getPubstateValue(CALLS_COMMAND);
    EXPECT_TRUE(calls.empty());
}

TEST_F(GameStateHelperTest, testCallsIfNotEmpty)
{
    makeBidding();
    const auto j = getPubstateValue(CALLS_COMMAND);
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
    const auto declarer = getPubstateValue(DECLARER_COMMAND);
    EXPECT_TRUE(declarer.empty());
}

TEST_F(GameStateHelperTest, testDeclarerIfBiddingCompleted)
{
    makeBidding();
    const auto declarer = getPubstateValue(DECLARER_COMMAND).get<Position>();
    EXPECT_EQ(Positions::NORTH, declarer);
}

TEST_F(GameStateHelperTest, testContractIfBiddingNotCompleted)
{
    const auto contract = getPubstateValue(CONTRACT_COMMAND);
    EXPECT_TRUE(contract.empty());
}

TEST_F(GameStateHelperTest, testContractIfBiddingCompleted)
{
    makeBidding();
    const auto contract = getPubstateValue(CONTRACT_COMMAND).get<Bridge::Contract>();
    EXPECT_EQ(CONTRACT, contract);
}

TEST_F(GameStateHelperTest, testAllowedCardsForPlayerInTurn)
{
    makeBidding();
    const auto cards = getSelfValue(*players[1], ALLOWED_CARDS_COMMAND)
        .get<CardVector>();
    EXPECT_TRUE(
        std::equal(
            cardTypeIterator(N_CARDS_PER_PLAYER),
            cardTypeIterator(2 * N_CARDS_PER_PLAYER),
            cards.begin(), cards.end()));
}

TEST_F(GameStateHelperTest, testAllowedCardsForPlayerNotInTurn)
{
    makeBidding();
    const auto cards = getSelfValue(*players[0], ALLOWED_CARDS_COMMAND);
    EXPECT_TRUE(cards.empty());
}

TEST_F(GameStateHelperTest, testAllowedCardsBeforeBiddingIsCompleted)
{
    const auto cards = getSelfValue(*players[0], ALLOWED_CARDS_COMMAND);
    EXPECT_TRUE(cards.empty());
}

TEST_F(GameStateHelperTest, testPublicCardsIfNotEmpty)
{
    const auto j = getPubstateValue(CARDS_COMMAND);
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
    const auto j = getPrivstateValue(*players[0], CARDS_COMMAND);
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
    const auto tricks = getPubstateValue(TRICKS_COMMAND);
    EXPECT_TRUE(tricks.empty());
}

TEST_F(GameStateHelperTest, testCurrentTrickIfNotEmpty)
{
    makeBidding();
    const auto& hand = dereference(engine->getCurrentDeal()).getHand(Positions::EAST);
    const auto expected_card_type =
        dereference(dereference(hand.getCard(0)).getType());
    engine->play(*players[1], hand, 0);
    const auto tricks = getPubstateValue(TRICKS_COMMAND);
    ASSERT_EQ(1u, tricks.size());
    const auto trick = tricks.front();
    const auto cards_iter = trick.find(CARDS_COMMAND);
    ASSERT_NE(trick.end(), cards_iter);
    ASSERT_EQ(1u, cards_iter->size());
    const auto& card_position = cards_iter->front();
    const auto position_iter = card_position.find(POSITION_COMMAND);
    ASSERT_NE(card_position.end(), position_iter);
    EXPECT_EQ(Positions::EAST, position_iter->get<Position>());
    const auto card_iter = card_position.find(CARD_COMMAND);
    ASSERT_NE(card_position.end(), card_iter);
    EXPECT_EQ(expected_card_type, card_iter->get<CardType>());
}

TEST_F(GameStateHelperTest, testVulnerability)
{
    const auto vulnerability = getPubstateValue(VULNERABILITY_COMMAND)
        .get<Bridge::Vulnerability>();
    EXPECT_EQ(Bridge::Vulnerability(false, false), vulnerability);
}
