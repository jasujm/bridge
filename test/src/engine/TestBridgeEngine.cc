#include "bridge/BasicHand.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/Bid.hh"
#include "bridge/BidIterator.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/CardsForPosition.hh"
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
#include "MockHand.hh"
#include "MockObserver.hh"
#include "Enumerate.hh"
#include "Utility.hh"
#include "Zip.hh"

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
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

using namespace Bridge;
using Engine::BridgeEngine;

class BridgeEngineTest : public testing::Test {
private:
    auto cardsForFunctor(const Position position)
    {
        return [this, position]() {
            auto ns = cardsFor(position);
            auto hand = std::make_shared<BasicHand>(
                containerAccessIterator(ns.begin(), cards),
                containerAccessIterator(ns.end(), cards));
            hand->subscribe(cardRevealStateObserver);
            hands.emplace(ns.front(), *hand);
            return hand;
        };
    }

protected:
    virtual void SetUp()
    {
        for (const auto t : zip(POSITIONS, players)) {
            engine.setPlayer(t.get<0>(), t.get<1>());
        }
        for (const auto e : enumerate(cards)) {
            ON_CALL(e.second, handleGetType())
                .WillByDefault(Return(enumerateCardType(e.first)));
            ON_CALL(e.second, handleIsKnown()).WillByDefault(Return(true));
        }
        for (const auto position : POSITIONS) {
            ON_CALL(
                *cardManager,
                handleGetHand(ElementsAreArray(cardsFor(position))))
                .WillByDefault(InvokeWithoutArgs(cardsForFunctor(position)));
        }
        ON_CALL(*cardManager, handleSubscribe(_))
            .WillByDefault(
                Invoke(
                    [this](auto observer)
                    {
                        shuffledNotifier.subscribe(std::move(observer));
                    }));
        ON_CALL(*cardManager, handleIsShuffleCompleted())
            .WillByDefault(Return(true));
        ON_CALL(*cardManager, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
        EXPECT_CALL(*cardManager, handleRequestShuffle());
        engine.initiate();
        Mock::VerifyAndClearExpectations(cardManager.get());
        shuffledNotifier.notifyAll(
            Engine::CardManager::ShufflingState::REQUESTED);
        ON_CALL(
            *cardRevealStateObserver,
            handleNotify(Hand::CardRevealState::REQUESTED, _))
            .WillByDefault(
                Invoke(
                    [this](const auto, const auto range)
                    {
                        for (auto& hand : hands) {
                            hand.second.get().reveal(
                                range.begin(), range.end());
                        }
                    }));
    }

    void updateExpectedStateAfterPlay(const Player& player)
    {
        const auto position = engine.getPosition(player);
        auto& cards = expectedState.cards->at(position);
        const auto new_position = clockwise(position);
        expectedState.positionInTurn = new_position;
        expectedState.currentTrick->emplace_back(position, cards.front());
        cards.erase(cards.begin());
    }

    void playCard(
        const Player& player, std::size_t card, bool revealDummy = false,
        bool completeTrick = false)
    {
        const auto position = engine.getPosition(player);
        const auto partner_position = partnerFor(position);
        const auto& partner = engine.getPlayer(partner_position);
        const auto& hand = dereference(engine.getHand(position));
        const auto& partner_hand = dereference(engine.getHand(partner_position));

        EXPECT_CALL(
            *cardRevealStateObserver,
            handleNotify(
                Hand::CardRevealState::REQUESTED,
                ElementsAre(card))).Times(AtLeast(1));
        EXPECT_CALL(
            *cardRevealStateObserver,
            handleNotify(
                Hand::CardRevealState::COMPLETED,
                ElementsAre(card))).Times(AtLeast(1));

        if (revealDummy) {
            EXPECT_CALL(
                *cardRevealStateObserver,
                handleNotify(
                    Hand::CardRevealState::REQUESTED,
                    ElementsAreArray(to(N_CARDS_PER_PLAYER))))
                .Times(AtLeast(1));
            EXPECT_CALL(
                *cardRevealStateObserver,
                handleNotify(
                    Hand::CardRevealState::COMPLETED,
                    ElementsAreArray(to(N_CARDS_PER_PLAYER))))
                .Times(AtLeast(1));
        }

        const auto trick_completed_observer =
            std::make_shared<MockObserver<BridgeEngine::TrickCompleted>>();
        engine.subscribeToTrickCompleted(trick_completed_observer);
        if (completeTrick) {
            const auto& trick = dereference(engine.getCurrentTrick());
            EXPECT_CALL(
                *trick_completed_observer,
                handleNotify(
                    BridgeEngine::TrickCompleted {trick, hands.at(0).get()}));
        } else {
            EXPECT_CALL(*trick_completed_observer, handleNotify(_)).Times(0);
        }

        engine.play(player, hand, card);
        engine.play(partner, hand, card);
        engine.play(player, partner_hand, card);
        engine.play(partner, partner_hand, card);
        Mock::VerifyAndClearExpectations(cardRevealStateObserver.get());
    }

    void assertDealState(boost::optional<Position> dummy = boost::none)
    {
        // Deal states for different positions (remove all visible cards
        // except position and dummy)
        for (const auto position : POSITIONS) {
            auto state = expectedState;
            if (state.cards) {
                for (auto iter = state.cards->begin();
                     iter != state.cards->end(); ) {
                    if (iter->first != position && iter->first != dummy) {
                        iter = state.cards->erase(iter);
                    } else {
                        ++iter;
                    }
                }
            }
            if (dummy && dummy == state.positionInTurn) {
                state.positionInTurn = partnerFor(*dummy);
            }
            EXPECT_EQ(
                state, makeDealState(engine, engine.getPlayer(position)));
        }
    }

    void assertNoHands()
    {
        for (const auto position : POSITIONS) {
            EXPECT_FALSE(engine.getHand(position));
        }
    }

    void assertHandsVisible(const Player* dummy = nullptr)
    {
        for (const auto player_position : POSITIONS) {
            const auto& player = engine.getPlayer(player_position);
            for (const auto hand_position : POSITIONS) {
                const auto& hand_player = engine.getPlayer(hand_position);
                const auto& hand = dereference(engine.getHand(hand_position));
                EXPECT_EQ(
                    player_position == hand_position || dummy == &hand_player,
                    engine.isVisible(hand, player));
            }
        }
    }

    void addTrickToNorthSouth()
    {
        expectedState.currentTrick->clear();
        ++expectedState.tricksWon->tricksWonByNorthSouth;
    }

    std::array<NiceMock<MockCard>, N_CARDS> cards;
    const std::shared_ptr<Engine::MockCardManager> cardManager {
        std::make_shared<NiceMock<Engine::MockCardManager>>()};
    const std::shared_ptr<Engine::MockGameManager> gameManager {
        std::make_shared<NiceMock<Engine::MockGameManager>>()};
    std::array<std::shared_ptr<Player>, N_PLAYERS> players {{
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>()}};
    BridgeEngine engine {cardManager, gameManager};
    Observable<Engine::CardManager::ShufflingState> shuffledNotifier;
    std::map<std::size_t, std::reference_wrapper<BasicHand>> hands;
    std::shared_ptr<NiceMock<MockCardRevealStateObserver>>
    cardRevealStateObserver {
        std::make_shared<NiceMock<MockCardRevealStateObserver>>()};
    DealState expectedState;
};

TEST_F(BridgeEngineTest, testPlayers)
{
    for (const auto position : POSITIONS) {
        EXPECT_EQ(position, engine.getPosition(engine.getPlayer(position)));
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
    assertDealState();
    assertNoHands();

    // Shuffling
    {
        auto observer = std::make_shared<
            MockObserver<BridgeEngine::ShufflingCompleted>>();
        EXPECT_CALL(*observer, handleNotify(_));
        engine.subscribeToShufflingCompleted(observer);
        shuffledNotifier.notifyAll(
            Engine::CardManager::ShufflingState::COMPLETED);
    }
    expectedState.stage = Stage::BIDDING;
    expectedState.positionInTurn = Position::NORTH;
    expectedState.cards.emplace();
    expectedState.calls.emplace();
    for (const auto position : POSITIONS) {
        // TODO: Make this prettier
        const auto hand = engine.getHand(position);
        ASSERT_TRUE(hand);
        auto card_types = std::vector<CardType> {};
        for (const auto& card : *hand) {
            card_types.emplace_back(*card.getType());
        }
        expectedState.cards->emplace(position, std::move(card_types));
    }

    // Bidding
    const auto calls = {
        Call {Pass {}}, Call {BID}, Call {Double {}}, Call {Redouble {}},
        Call {Pass {}}, Call {Pass {}}, Call {Pass {}}};
    for (const auto e : enumerate(calls)) {
        const auto& call = e.second;
        assertDealState();
        assertHandsVisible();
        const auto& player = *players[e.first % players.size()];
        const auto position = engine.getPosition(player);
        auto observer = std::make_shared<
            MockObserver<BridgeEngine::BiddingCompleted>>();
        EXPECT_CALL(*observer, handleNotify(_)).Times(
            static_cast<std::size_t>(e.first + 1) == calls.size() ? 1 : 0);
        engine.subscribeToBiddingCompleted(observer);
        engine.call(player, call);
        expectedState.calls->emplace_back(position, call);
        expectedState.positionInTurn.emplace(clockwise(position));
    }

    // Playing
    expectedState.stage = Stage::PLAYING;
    expectedState.positionInTurn = Position::SOUTH;
    expectedState.declarer = Position::EAST;
    expectedState.contract.emplace(BID, Doubling::REDOUBLED);
    expectedState.currentTrick.emplace();
    expectedState.tricksWon.emplace(0, 0);
    for (const auto i : to(players.size())) {
        assertDealState(
            i == 0 ? boost::none : boost::make_optional(Position::WEST));
        const auto turn_i = (i + 2) % players.size();
        auto& player = *players[turn_i % players.size()];
        playCard(player, 0, i == 0, i == players.size() - 1);
        updateExpectedStateAfterPlay(player);
        assertHandsVisible(&engine.getPlayer(Position::WEST));
    }

    expectedState.positionInTurn = Position::NORTH;
    addTrickToNorthSouth();
    for (const auto i : from_to(1u, N_CARDS_PER_PLAYER)) {
        for (const auto& player : players) {
            auto observer =
                std::make_shared<MockObserver<BridgeEngine::DealEnded>>();
            const auto last_card_in_trick = &player == &players.back();
            const auto notify_count =
                (i == N_CARDS_PER_PLAYER - 1 && last_card_in_trick) ?
                1 : 0;
            EXPECT_CALL(*observer, handleNotify(_)).Times(notify_count);
            assertDealState(Position::WEST);
            assertHandsVisible(&engine.getPlayer(Position::WEST));

            engine.subscribeToDealEnded(observer);
            playCard(*player, i, false, last_card_in_trick);
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

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    for (const auto& player : players) {
        auto observer = std::make_shared<
            MockObserver<BridgeEngine::BiddingCompleted>>();
        engine.subscribeToBiddingCompleted(observer);
        EXPECT_CALL(*observer, handleNotify(_)).Times(0);
        engine.call(*player, Pass {});
    }
    EXPECT_FALSE(engine.hasEnded());
}

TEST_F(BridgeEngineTest, testEndGame)
{
    // Just one deal as game ends
    EXPECT_CALL(*cardManager, handleRequestShuffle()).Times(0);
    ON_CALL(*gameManager, handleHasEnded()).WillByDefault(Return(false));

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);

    Mock::VerifyAndClearExpectations(cardManager.get());
    ON_CALL(*gameManager, handleHasEnded()).WillByDefault(Return(true));

    for (const auto& player : players) {
        engine.call(*player, Pass {});
    }
    EXPECT_TRUE(engine.hasEnded());
}

TEST_F(BridgeEngineTest, testSuccessfulCall)
{
    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    auto observer = std::make_shared<MockObserver<BridgeEngine::CallMade>>();
    const auto& player = *players.front();
    const auto call = Bid {1, Strain::CLUBS};
    EXPECT_CALL(*observer, handleNotify(BridgeEngine::CallMade {player, call}));
    engine.subscribeToCallMade(observer);
    EXPECT_TRUE(engine.call(player, call));
}

TEST_F(BridgeEngineTest, testFailedCall)
{
    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    auto observer = std::make_shared<MockObserver<BridgeEngine::CallMade>>();
    const auto& player = *players.back();
    const auto call = Bid {1, Strain::CLUBS};
    EXPECT_CALL(*observer, handleNotify(_)).Times(0);
    engine.subscribeToCallMade(observer);
    EXPECT_FALSE(engine.call(player, call));
}

TEST_F(BridgeEngineTest, testSuccessfulPlay)
{
    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    engine.call(*players[0], Bid {1, Strain::CLUBS});
    engine.call(*players[1], Pass {});
    engine.call(*players[2], Pass {});
    engine.call(*players[3], Pass {});

    auto observer = std::make_shared<MockObserver<BridgeEngine::CardPlayed>>();
    const auto& hand = dereference(engine.getHandInTurn());
    EXPECT_CALL(
        *observer,
        handleNotify(
            BridgeEngine::CardPlayed {
                *players[1], hand, dereference(hand.getCard(0))}));
    engine.subscribeToCardPlayed(observer);
    ASSERT_TRUE(
        engine.play(*players[1], hand, 0));
}

TEST_F(BridgeEngineTest, testFailedPlay)
{
    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    engine.call(*players[0], Bid {1, Strain::CLUBS});
    engine.call(*players[1], Pass {});
    engine.call(*players[2], Pass {});
    engine.call(*players[3], Pass {});
    auto observer = std::make_shared<MockObserver<BridgeEngine::CardPlayed>>();
    EXPECT_CALL(*observer, handleNotify(_)).Times(0);
    engine.subscribeToCardPlayed(observer);
    ASSERT_FALSE(
        engine.play(*players[2], dereference(engine.getHandInTurn()), 0));
}
