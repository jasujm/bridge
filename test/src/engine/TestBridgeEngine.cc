#include "bridge/BasicHand.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/Bid.hh"
#include "bridge/BidIterator.hh"
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
#include "MockObserver.hh"
#include "Enumerate.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <utility>

using testing::_;
using testing::AtLeast;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

using namespace Bridge;
using Engine::BridgeEngine;

namespace {

template<typename Iterator>
bool isInVisibleHands(
    Iterator first, Iterator last, const BridgeEngine& engine,
    const Player& player)
{
    const auto& hand = dereference(engine.getHand(player));
    return std::find_if(first, last, compareAddress(hand)) != last;
}

}

class BridgeEngineTest : public testing::Test {
private:
    auto cardsForFunctor(const Position position)
    {
        return [&cards = cards, position]() {
            auto ns = cardsFor(position);
            return std::make_unique<BasicHand>(
                containerAccessIterator(ns.begin(), cards),
                containerAccessIterator(ns.end(), cards));
        };
    }

protected:
    virtual void SetUp()
    {
        for (const auto e : enumerate(cards))
        {
            ON_CALL(e.second, handleGetType())
                .WillByDefault(Return(enumerateCardType(e.first)));
            ON_CALL(e.second, handleIsKnown()).WillByDefault(Return(true));
        }
        for (const auto position : POSITIONS) {
            ON_CALL(*cardManager, handleGetHand(cardsFor(position)))
                .WillByDefault(InvokeWithoutArgs(cardsForFunctor(position)));
        }
        ON_CALL(*cardManager, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
        EXPECT_CALL(*cardManager, handleRequestShuffle());
        engine = std::make_unique<BridgeEngine>(
            cardManager, gameManager, players.begin(), players.end());
        Mock::VerifyAndClearExpectations(engine.get());
    }

    void updateAllowedCards(Position position)
    {
        expectedState.allowedCards = expectedState.cards->at(position);
    }

    void updateAllowedCalls(
        bool doublingAllowed, bool redoublingAllowed,
        const Bid& lowestAllowedBid)
    {
        expectedState.allowedCalls = DealState::AllowedCalls {Pass {}};
        if (doublingAllowed) {
            expectedState.allowedCalls->emplace_back(Double {});
        }
        if (redoublingAllowed) {
            expectedState.allowedCalls->emplace_back(Redouble {});
        }
        std::copy(
            BidIterator(lowestAllowedBid), BidIterator(boost::none),
            std::back_inserter(*expectedState.allowedCalls));
    }

    void updateExpectedStateAfterPlay(const Player& player)
    {
        const auto position = engine->getPosition(player);
        auto& cards = expectedState.cards->at(position);
        const auto new_position = clockwise(position);
        expectedState.positionInTurn = new_position;
        updateAllowedCards(new_position);
        expectedState.currentTrick->emplace_back(position, cards.front());
        cards.erase(cards.begin());
    }

    void assertHandsVisible(bool ownVisible, const Player* dummy = nullptr)
    {
        for (const auto position : POSITIONS) {
            const auto& player = engine->getPlayer(position);
            auto hands = std::vector<std::reference_wrapper<const Hand>> {};
            engine->getVisibleHands(player, std::back_inserter(hands));
            EXPECT_TRUE(
                !ownVisible ||
                isInVisibleHands(hands.begin(), hands.end(), *engine, player));
            EXPECT_TRUE(
                dummy == nullptr ||
                isInVisibleHands(hands.begin(), hands.end(), *engine, *dummy));
        }
    }

    void addTrickToNorthSouth()
    {
        expectedState.currentTrick->clear();
        ++expectedState.tricksWon->tricksWonByNorthSouth;
    }

    std::array<NiceMock<MockCard>, N_CARDS> cards;
    std::array<std::shared_ptr<BasicHand>, N_POSITIONS> hands;
    const std::shared_ptr<Engine::MockCardManager> cardManager {
        std::make_shared<NiceMock<Engine::MockCardManager>>()};
    const std::shared_ptr<Engine::MockGameManager> gameManager {
        std::make_shared<NiceMock<Engine::MockGameManager>>()};
    std::array<std::shared_ptr<Player>, N_PLAYERS> players {{
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>()}};
    std::unique_ptr<BridgeEngine> engine;
    DealState expectedState;
};

TEST_F(BridgeEngineTest, testInvalidConstruction)
{
    EXPECT_THROW(
        (BridgeEngine {
            cardManager, gameManager, players.begin(), players.end() - 1}),
        std::invalid_argument);
}

TEST_F(BridgeEngineTest, testPlayers)
{
    for (const auto position : POSITIONS) {
        EXPECT_EQ(position, engine->getPosition(engine->getPlayer(position)));
    }
}

TEST_F(BridgeEngineTest, testBridgeEngine)
{
    // TODO: Could this test be less ugly?
    constexpr Bid BID {7, Strain::CLUBS};

    EXPECT_CALL(*cardManager, handleRequestShuffle());
    EXPECT_CALL(
        *gameManager, handleAddResult(
            Partnership::EAST_WEST, Contract {BID, Doubling::REDOUBLED}, 0));
    EXPECT_CALL(*gameManager, handleGetOpenerPosition())
        .WillRepeatedly(Return(Position::NORTH));
    EXPECT_CALL(*gameManager, handleGetVulnerability())
        .WillRepeatedly(Return(Vulnerability {true, true}));

    // Startup
    expectedState.stage = Stage::SHUFFLING;
    expectedState.vulnerability.emplace(true, true);
    ASSERT_EQ(expectedState, makeDealState(*engine));
    assertHandsVisible(false);

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
        expectedState.cards->emplace(position, std::move(card_types));
    }

    // Bidding
    const auto next_bid = nextHigherBid(BID).value();
    const auto calls = {
        std::make_tuple(Call {Pass {}},     false, false, Bid::LOWEST_BID),
        std::make_tuple(Call {BID},         false, false, Bid::LOWEST_BID),
        std::make_tuple(Call {Double {}},   true,  false, next_bid),
        std::make_tuple(Call {Redouble {}}, false, true,  next_bid),
        std::make_tuple(Call {Pass {}},     false, false, next_bid),
        std::make_tuple(Call {Pass {}},     false, false, next_bid),
        std::make_tuple(Call {Pass {}},     false, false, next_bid),
    };
    for (const auto e : enumerate(calls)) {
        const auto& call = std::get<0>(e.second);
        updateAllowedCalls(
            std::get<1>(e.second), std::get<2>(e.second),
            std::get<3>(e.second));
        ASSERT_EQ(expectedState, makeDealState(*engine));
        assertHandsVisible(true);
        const auto& player = *players[e.first % players.size()];
        const auto position = engine->getPosition(player);
        engine->call(player, call);
        expectedState.calls->emplace_back(position, call);
        expectedState.positionInTurn.emplace(clockwise(position));
    }

    // Playing
    expectedState.stage = Stage::PLAYING;
    expectedState.positionInTurn = Position::SOUTH;
    expectedState.allowedCalls = boost::none;
    updateAllowedCards(Position::SOUTH);
    expectedState.declarer = Position::EAST;
    expectedState.contract.emplace(BID, Doubling::REDOUBLED);
    expectedState.currentTrick.emplace();
    expectedState.tricksWon.emplace(0, 0);
    for (const auto i : to(players.size())) {
        ASSERT_EQ(expectedState, makeDealState(*engine));
        const auto turn_i = (i + 2) % players.size();
        auto& player = *players[turn_i % players.size()];
        engine->play(player, 0);
        updateExpectedStateAfterPlay(player);
        assertHandsVisible(true, &engine->getPlayer(Position::WEST));
    }

    expectedState.positionInTurn = Position::NORTH;
    updateAllowedCards(Position::NORTH);
    addTrickToNorthSouth();
    for (const auto i : from_to(1u, N_CARDS_PER_PLAYER)) {
        for (const auto& player : players) {
            auto observer =
                std::make_shared<MockObserver<BridgeEngine::DealEnded>>();
            const auto notify_count = (
                i == N_CARDS_PER_PLAYER - 1 &&
                &player == &players.back()) ? 1 : 0;
            EXPECT_CALL(*observer, handleNotify(_)).Times(notify_count);
            ASSERT_EQ(expectedState, makeDealState(*engine));
            assertHandsVisible(true, &engine->getPlayer(Position::WEST));

            engine->subscribe(observer);
            engine->play(*player, i);
            updateExpectedStateAfterPlay(*player);
        }
        addTrickToNorthSouth();
    }
}

TEST_F(BridgeEngineTest, testPassOut)
{
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
