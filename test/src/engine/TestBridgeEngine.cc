#include "bridge/BasicHand.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "engine/BridgeEngine.hh"
#include "engine/MakeDealState.hh"
#include "MockCard.hh"
#include "MockCardManager.hh"
#include "MockGameManager.hh"
#include "Enumerate.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

using testing::_;
using testing::AtLeast;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

using Bridge::N_CARDS;
using Bridge::N_CARDS_PER_PLAYER;
using Bridge::N_POSITIONS;
using Bridge::N_PLAYERS;
using Bridge::from_to;
using Bridge::enumerate;
using Bridge::to;

class BridgeEngineTest : public testing::Test {
private:
    auto cardsForFunctor(const Bridge::Position position)
    {
        return [&cards = cards, position]() {
            auto ns = cardsFor(position);
            return std::make_unique<Bridge::BasicHand>(
                Bridge::containerAccessIterator(ns.begin(), cards),
                Bridge::containerAccessIterator(ns.end(), cards));
        };
    }

protected:
    virtual void SetUp()
    {
        for (const auto e : enumerate(cards))
        {
            ON_CALL(e.second, handleGetType())
                .WillByDefault(Return(Bridge::enumerateCardType(e.first)));
            ON_CALL(e.second, handleIsKnown()).WillByDefault(Return(true));
        }
        for (const auto position : Bridge::POSITIONS) {
            ON_CALL(*cardManager, handleGetHand(cardsFor(position)))
                .WillByDefault(InvokeWithoutArgs(cardsForFunctor(position)));
        }
        ON_CALL(*cardManager, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
        EXPECT_CALL(*cardManager, handleRequestShuffle());
        engine = std::make_unique<Bridge::Engine::BridgeEngine>(
            cardManager, gameManager, players.begin(), players.end());
        Mock::VerifyAndClearExpectations(engine.get());
    }

    void updateExpectedStateAfterPlay(const Bridge::Player& player)
    {
        const auto position = engine->getPosition(player);
        auto& cards = expectedState.cards->at(position);
        expectedState.positionInTurn.emplace(clockwise(position));
        expectedState.currentTrick->emplace_back(position, cards.front());
        cards.erase(cards.begin());
    }

    void addTrickToNorthSouth()
    {
        expectedState.currentTrick->clear();
        ++expectedState.tricksWon->tricksWonByNorthSouth;
    }

    std::array<NiceMock<Bridge::MockCard>, N_CARDS> cards;
    std::array<std::shared_ptr<Bridge::BasicHand>, N_POSITIONS> hands;
    const std::shared_ptr<Bridge::Engine::MockCardManager> cardManager {
        std::make_shared<NiceMock<Bridge::Engine::MockCardManager>>()};
    const std::shared_ptr<Bridge::Engine::MockGameManager> gameManager {
        std::make_shared<NiceMock<Bridge::Engine::MockGameManager>>()};
    std::array<std::shared_ptr<Bridge::Player>, N_PLAYERS> players {{
        std::make_shared<Bridge::BasicPlayer>(),
        std::make_shared<Bridge::BasicPlayer>(),
        std::make_shared<Bridge::BasicPlayer>(),
        std::make_shared<Bridge::BasicPlayer>()}};
    std::unique_ptr<Bridge::Engine::BridgeEngine> engine;
    Bridge::DealState expectedState;
};

TEST_F(BridgeEngineTest, testInvalidConstruction)
{
    EXPECT_THROW(
        (Bridge::Engine::BridgeEngine {
            cardManager, gameManager, players.begin(), players.end() - 1}),
        std::invalid_argument);
}

TEST_F(BridgeEngineTest, testPlayers)
{
    for (const auto position : Bridge::POSITIONS) {
        EXPECT_EQ(position, engine->getPosition(engine->getPlayer(position)));
    }
}

TEST_F(BridgeEngineTest, testBridgeEngine)
{
    // TODO: Could this test be less ugly?
    using namespace Bridge;
    constexpr Bid BID {7, Strain::CLUBS};

    EXPECT_CALL(*cardManager, handleRequestShuffle());
    EXPECT_CALL(
        *gameManager, handleAddResult(
            Partnership::EAST_WEST, Contract {BID, Doubling::UNDOUBLED}, 0));
    EXPECT_CALL(*gameManager, handleGetOpenerPosition())
        .WillRepeatedly(Return(Position::NORTH));
    EXPECT_CALL(*gameManager, handleGetVulnerability())
        .WillRepeatedly(Return(Vulnerability {true, true}));

    // Startup
    expectedState.stage = Stage::SHUFFLING;
    expectedState.vulnerability.emplace(true, true);
    ASSERT_EQ(expectedState, makeDealState(*engine));

    // Shuffling
    cardManager->notifyAll(Engine::Shuffled {});
    expectedState.stage = Stage::BIDDING;
    expectedState.positionInTurn = Position::NORTH;
    expectedState.cards.emplace();
    expectedState.calls.emplace();
    for (const auto position : POSITIONS) {
        // TODO: Make this prettier
        const auto hand = engine->getHand(engine->getPlayer(position));
        ASSERT_TRUE(hand);
        auto card_types = std::vector<CardType> {};
        for (const auto& card : *hand) {
            card_types.emplace_back(*card.getType());
        }
        expectedState.cards->emplace(position, card_types);
    }

    // Bidding
    const auto calls = std::vector<Call>{
        Pass {}, BID, Pass {}, Pass {}, Pass {}};
    for (const auto e : enumerate(calls)) {
        ASSERT_EQ(expectedState, makeDealState(*engine));
        const auto& player = *players[e.first % players.size()];
        const auto position = engine->getPosition(player);
        engine->call(player, e.second);
        expectedState.calls->emplace_back(position, e.second);
        expectedState.positionInTurn.emplace(clockwise(position));
    }

    // Playing
    expectedState.stage = Stage::PLAYING;
    expectedState.positionInTurn.emplace(Position::SOUTH);
    expectedState.declarer.emplace(Position::EAST);
    expectedState.contract.emplace(BID, Doubling::UNDOUBLED);
    expectedState.currentTrick.emplace();
    expectedState.tricksWon.emplace(0, 0);
    for (const auto i : to(players.size())) {
        ASSERT_EQ(expectedState, makeDealState(*engine));
        const auto turn_i = (i + 2) % players.size();
        auto& player = *players[turn_i % players.size()];
        engine->play(player, 0);
        updateExpectedStateAfterPlay(player);
    }

    expectedState.positionInTurn.emplace(Position::NORTH);
    addTrickToNorthSouth();
    for (const auto i : from_to(1u, N_CARDS_PER_PLAYER)) {
        for (const auto& player : players) {
            ASSERT_EQ(expectedState, makeDealState(*engine));
            engine->play(*player, i);
            updateExpectedStateAfterPlay(*player);
        }
        addTrickToNorthSouth();
    }
}

TEST_F(BridgeEngineTest, testPassOut)
{
    using namespace Bridge;

    // Two deals as game does not end
    EXPECT_CALL(*cardManager, handleRequestShuffle());
    EXPECT_CALL(*gameManager, handleAddPassedOut());

    cardManager->notifyAll(Engine::Shuffled {});
    for (const auto& player : players) {
        engine->call(*player, Pass {});
    }
    EXPECT_FALSE(engine->hasEnded());
}

TEST_F(BridgeEngineTest, testEndGame)
{
    using namespace Bridge;

    // Just one deal as game ends
    EXPECT_CALL(*cardManager, handleRequestShuffle()).Times(0);
    ON_CALL(*gameManager, handleHasEnded()).WillByDefault(Return(false));

    cardManager->notifyAll(Engine::Shuffled {});

    Mock::VerifyAndClearExpectations(engine.get());
    ON_CALL(*gameManager, handleHasEnded()).WillByDefault(Return(true));

    for (const auto& player : players) {
        engine->call(*player, Pass {});
    }
    EXPECT_TRUE(engine->hasEnded());
}
