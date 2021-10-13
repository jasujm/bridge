// FIXME: The interface of BridgeEngine has evolved quite a lot, this
// unit test hasn't. Maybe this needs a complete rewrite to properly
// use the Deal interface etc. instead of this unholy mess

#include "bridge/BasicHand.hh"
#include "bridge/Bid.hh"
#include "bridge/BidIterator.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Contract.hh"
#include "bridge/Deal.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/SimpleCard.hh"
#include "bridge/Vulnerability.hh"
#include "engine/BridgeEngine.hh"
#include "engine/MakeDealState.hh"
#include "MockBidding.hh"
#include "MockCard.hh"
#include "MockCardManager.hh"
#include "MockDeal.hh"
#include "MockGameManager.hh"
#include "MockHand.hh"
#include "MockObserver.hh"
#include "MockPlayer.hh"
#include "MockTrick.hh"
#include "Enumerate.hh"
#include "TestUtility.hh"
#include "Utility.hh"

#include <boost/range/combine.hpp>
#include <boost/uuid/string_generator.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
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
using testing::Field;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;

using namespace Bridge;
using Engine::BridgeEngine;

namespace {
boost::uuids::string_generator gen;
const auto UUID = gen("97431d93-cd58-482f-8d97-b22c7f2bc73f");
constexpr auto BID = Bid {7, Strains::CLUBS};
}

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
    void setupDependencies()
    {
        for (const auto t : boost::combine(Position::all(), players)) {
            engine.setPlayer(t.get<0>(), t.get<1>());
        }
        for (const auto e : enumerate(cards)) {
            ON_CALL(e.second, handleGetType())
                .WillByDefault(Return(enumerateCardType(e.first)));
            ON_CALL(e.second, handleIsKnown()).WillByDefault(Return(true));
        }
        for (const auto position : Position::all()) {
            ON_CALL(
                *cardManager,
                handleGetHand(ElementsAreArray(cardsFor(position))))
                .WillByDefault(InvokeWithoutArgs(cardsForFunctor(position)));
        }
        ON_CALL(*cardManager, handleIsShuffleCompleted())
            .WillByDefault(Return(true));
        ON_CALL(*cardManager, handleGetNumberOfCards())
            .WillByDefault(Return(N_CARDS));
        ON_CALL(*gameManager, handleGetOpenerPosition())
            .WillByDefault(Return(Positions::NORTH));
        ON_CALL(*gameManager, handleGetVulnerability())
            .WillByDefault(Return(Vulnerability {true, true}));
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

    void startDeal()
    {
        EXPECT_CALL(*cardManager, handleRequestShuffle());
        engine.startDeal();
        Mock::VerifyAndClearExpectations(cardManager.get());
        shuffledNotifier.notifyAll(
            Engine::CardManager::ShufflingState::REQUESTED);
    }

    void updateExpectedStateAfterPlay(const Player& player)
    {
        const auto position = dereference(engine.getPosition(player));
        auto& cards = expectedState.cards->at(position);
        const auto new_position = clockwise(position);
        expectedState.positionInTurn = new_position;
        expectedState.currentTrick->emplace_back(position, cards.front());
        cards.erase(cards.begin());
    }

    void playCard(
        const Deal& deal, const Player& player, int card,
        bool revealDummy, bool completeTrick, int index)
    {
        const auto position = dereference(engine.getPosition(player));
        const auto partner_position = partnerFor(position);
        const auto& partner = dereference(engine.getPlayer(partner_position));
        const auto& hand = deal.getHand(position);
        const auto& partner_hand = deal.getHand(partner_position);

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
                    ElementsAreArray(
                        Bridge::vectorize(to(N_CARDS_PER_PLAYER)))))
                .Times(AtLeast(1));
            EXPECT_CALL(
                *cardRevealStateObserver,
                handleNotify(
                    Hand::CardRevealState::COMPLETED,
                    ElementsAreArray(
                        Bridge::vectorize(to(N_CARDS_PER_PLAYER)))))
                .Times(AtLeast(1));
        }

        const auto trick_completed_observer =
            std::make_shared<MockObserver<BridgeEngine::TrickCompleted>>();
        engine.subscribeToTrickCompleted(trick_completed_observer);
        if (completeTrick) {
            const auto& trick = dereference(deal.getCurrentTrick());
            EXPECT_CALL(
                *trick_completed_observer,
                handleNotify(
                    BridgeEngine::TrickCompleted {
                        dealUuid, trick, Positions::NORTH, index}));
        } else {
            EXPECT_CALL(*trick_completed_observer, handleNotify(_)).Times(0);
        }

        // No playing by someone not taking part in the the game
        {
            const MockPlayer other_player;
            EXPECT_FALSE(engine.play(other_player, hand, card));
        }

        engine.play(player, hand, card);
        engine.play(partner, hand, card);
        engine.play(player, partner_hand, card);
        engine.play(partner, partner_hand, card);
        engine.startDeal();
        Mock::VerifyAndClearExpectations(cardRevealStateObserver.get());
    }

    void assertDealState(std::optional<Position> dummy = std::nullopt)
    {
        // Deal states for different positions (remove all visible cards
        // except position and dummy)
        for (const auto position : Position::all()) {
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
                state,
                makeDealState(engine, dereference(engine.getPlayer(position))));
        }
    }

    void assertHandsVisible(const Deal& deal, const Player* dummy = nullptr)
    {
        for (const auto position : Position::all()) {
            const auto* player = engine.getPlayer(position);
            EXPECT_EQ(dummy == player, deal.isVisibleToAll(position));
        }
    }

    void addTrickToNorthSouth()
    {
        expectedState.currentTrick->clear();
    }

    void setupRecalledDeal()
    {
        // Cards are already shuffled when a deal is recalled
        EXPECT_CALL(*cardManager, handleRequestShuffle()).Times(0);
        for (const auto position : Position::all()) {
            EXPECT_CALL(
                *cardManager,
                handleGetHand(ElementsAreArray(cardsFor(position))));
        }

        ON_CALL(*recalledDeal, handleGetUuid()).WillByDefault(ReturnRef(UUID));

        // Setup hands in the deal
        ON_CALL(*recalledDeal, handleGetHand(_))
            .WillByDefault(
                Invoke(
                    [this](const auto p) -> const Hand&
                    {
                        return *hands_in_deal.at(p);
                    }));

        // Setup calls in the deal
        ON_CALL(bidding_in_deal, handleGetNumberOfCalls())
            .WillByDefault(Return(calls_in_deal.size()));
        ON_CALL(bidding_in_deal, handleGetOpeningPosition())
            .WillByDefault(Return(Positions::NORTH));
        ON_CALL(bidding_in_deal, handleGetCall(_))
            .WillByDefault(
                Invoke([this](const auto n) { return calls_in_deal.at(n); }));
        ON_CALL(*recalledDeal, handleGetBidding())
            .WillByDefault(ReturnRef(bidding_in_deal));

        // Setup tricks in the deal
        ON_CALL(*recalledDeal, handleGetNumberOfTricks())
            .WillByDefault(Return(tricks_in_deal.size()));
        ON_CALL(*recalledDeal, handleGetTrick(_))
            .WillByDefault(
                Invoke(
                    [this](const auto n) -> decltype(auto)
                    {
                        return tricks_in_deal.at(n);
                    }));
        ON_CALL(tricks_in_deal[0], handleGetNumberOfCardsPlayed())
            .WillByDefault(Return(cards_in_trick.size()));
        for (const auto [n, position] : enumerate(Position::all())) {
            ON_CALL(tricks_in_deal[0], handleGetHand(n))
                .WillByDefault(ReturnRef(*hands_in_deal.at(position)));
            ON_CALL(tricks_in_deal[0], handleGetCard(n))
                .WillByDefault(ReturnRef(cards_in_trick.at(n)));
        }
        ON_CALL(tricks_in_deal[1], handleGetHand(0))
            .WillByDefault(ReturnRef(*hands_in_deal.at(Positions::EAST)));
    }

    std::array<NiceMock<MockCard>, N_CARDS> cards;
    const std::shared_ptr<Engine::MockCardManager> cardManager {
        std::make_shared<NiceMock<Engine::MockCardManager>>()};
    Observable<Engine::CardManager::ShufflingState> shuffledNotifier {
        [this]()
        {
            auto notifier = Observable<Engine::CardManager::ShufflingState>();
            ON_CALL(*cardManager, handleSubscribe(_))
                .WillByDefault(
                    Invoke(
                        [this](auto observer)
                        {
                            shuffledNotifier.subscribe(std::move(observer));
                        }));
            return notifier;
        }()};
    const std::shared_ptr<Engine::MockGameManager> gameManager {
        std::make_shared<NiceMock<Engine::MockGameManager>>()};
    std::array<std::shared_ptr<Player>, N_PLAYERS> players {{
        std::make_shared<MockPlayer>(),
        std::make_shared<MockPlayer>(),
        std::make_shared<MockPlayer>(),
        std::make_shared<MockPlayer>()}};
    BridgeEngine engine {cardManager, gameManager};
    std::map<int, std::reference_wrapper<BasicHand>> hands;
    std::shared_ptr<NiceMock<MockCardRevealStateObserver>>
    cardRevealStateObserver {
        std::make_shared<NiceMock<MockCardRevealStateObserver>>()};
    DealState expectedState;
    Uuid dealUuid;

    // Recall tests
    std::unique_ptr<Bridge::MockDeal> recalledDeal {
        std::make_unique<NiceMock<Bridge::MockDeal>>()};
    std::map<Position, std::shared_ptr<MockHand>> hands_in_deal {
        std::pair { Positions::NORTH, std::make_shared<NiceMock<MockHand>>() },
        std::pair { Positions::EAST,  std::make_shared<NiceMock<MockHand>>() },
        std::pair { Positions::SOUTH, std::make_shared<NiceMock<MockHand>>() },
        std::pair { Positions::WEST,  std::make_shared<NiceMock<MockHand>>() },
    };
    NiceMock<Bridge::MockBidding> bidding_in_deal;
    std::array<Call, 4> calls_in_deal {
        BID,
        Pass {},
        Pass {},
        Pass {},
    };
    std::array<NiceMock<MockTrick>, 2> tricks_in_deal {
        NiceMock<MockTrick> {},
        NiceMock<MockTrick> {},
    };
    std::array<SimpleCard, 4> cards_in_trick {
        SimpleCard {CardType {Ranks::TWO, Suits::CLUBS}},
        SimpleCard {CardType {Ranks::TWO, Suits::DIAMONDS}},
        SimpleCard {CardType {Ranks::TWO, Suits::HEARTS}},
        SimpleCard {CardType {Ranks::TWO, Suits::SPADES}},
    };

};

TEST_F(BridgeEngineTest, testBridgeEngine)
{
    setupDependencies();
    startDeal();
    EXPECT_CALL(*cardManager, handleRequestShuffle());
    EXPECT_CALL(
        *gameManager, handleAddResult(
            Partnerships::EAST_WEST, Contract {BID, Doublings::REDOUBLED}, 0));

    // Startup
    expectedState.stage = Stage::SHUFFLING;
    assertDealState();
    ASSERT_FALSE(engine.getCurrentDeal());

    // Shuffling
    {
        auto deal_observer = std::make_shared<
            MockObserver<BridgeEngine::DealStarted>>();
        EXPECT_CALL(
            *deal_observer,
            handleNotify(
                Field(&BridgeEngine::DealStarted::opener, Positions::NORTH)));
        engine.subscribeToDealStarted(deal_observer);
        auto turn_observer = std::make_shared<
            MockObserver<BridgeEngine::TurnStarted>>();
        EXPECT_CALL(
            *turn_observer,
            handleNotify(
                Field(
                    &BridgeEngine::TurnStarted::position, Positions::NORTH)));
        engine.subscribeToTurnStarted(turn_observer);
        shuffledNotifier.notifyAll(
            Engine::CardManager::ShufflingState::COMPLETED);
    }
    const auto* deal = engine.getCurrentDeal();
    ASSERT_TRUE(deal);
    dealUuid = deal->getUuid();
    expectedState.vulnerability.emplace(true, true);
    expectedState.stage = Stage::BIDDING;
    expectedState.positionInTurn = Positions::NORTH;
    expectedState.cards.emplace();
    expectedState.calls.emplace();
    for (const auto position : Position::all()) {
        const auto& hand = deal->getHand(position);
        auto card_types = std::vector<CardType> {};
        for (const auto& card : hand) {
            card_types.emplace_back(*card.getType());
        }
        expectedState.cards->emplace(position, std::move(card_types));
    }

    // No bidding by someone not taking part in the the game
    {
        const MockPlayer other_player;
        EXPECT_FALSE(engine.call(other_player, BID));
    }

    // Bidding
    const auto calls = {
        Call {Pass {}}, Call {BID}, Call {Double {}}, Call {Redouble {}},
        Call {Pass {}}, Call {Pass {}}, Call {Pass {}}};
    for (const auto e : enumerate(calls)) {
        const auto& call = e.second;
        assertDealState();
        assertHandsVisible(*deal);
        const auto& player = *players[e.first % players.size()];
        const auto position = dereference(engine.getPosition(player));
        const auto is_last_call = (e.first + 1 == ssize(calls));
        auto turn_observer = std::make_shared<
            StrictMock<MockObserver<BridgeEngine::TurnStarted>>>();
        EXPECT_CALL(
            *turn_observer,
            handleNotify(
                BridgeEngine::TurnStarted {
                    // After last call south leads the first card
                    dealUuid,
                    is_last_call ? Positions::SOUTH : clockwise(position) }));
        auto bidding_observer = std::make_shared<
            StrictMock<MockObserver<BridgeEngine::BiddingCompleted>>>();
        auto trick_observer = std::make_shared<
            StrictMock<MockObserver<BridgeEngine::TrickStarted>>>();
        if (is_last_call) {
            EXPECT_CALL(
                *bidding_observer,
                handleNotify(
                    BridgeEngine::BiddingCompleted {
                        dealUuid, Positions::EAST,
                            Contract {BID, Doublings::REDOUBLED }}));
            EXPECT_CALL(
                *trick_observer,
                handleNotify(
                    BridgeEngine::TrickStarted {dealUuid, Positions::SOUTH }));
        }
        engine.subscribeToTurnStarted(turn_observer);
        engine.subscribeToBiddingCompleted(bidding_observer);
        engine.subscribeToTrickStarted(trick_observer);
        engine.call(player, call);
        expectedState.calls->emplace_back(position, call);
        expectedState.positionInTurn.emplace(clockwise(position));
    }

    // Playing
    expectedState.stage = Stage::PLAYING;
    expectedState.positionInTurn = Positions::SOUTH;
    expectedState.declarer = Positions::EAST;
    expectedState.contract.emplace(BID, Doublings::REDOUBLED);
    expectedState.currentTrick.emplace();
    std::array<Position, N_PLAYERS> next_positions_first_turn {
        Positions::EAST, Positions::NORTH, Positions::EAST, Positions::NORTH
    };
    for (const auto i : to(players.size())) {
        assertDealState(
            i == 0 ? std::nullopt : std::optional {Positions::WEST});
        const auto turn_i = (i + 2) % players.size();
        auto& player = *players[turn_i % players.size()];
        auto observer = std::make_shared<
            StrictMock<MockObserver<BridgeEngine::TurnStarted>>>();
        EXPECT_CALL(
            *observer,
            handleNotify(
                BridgeEngine::TurnStarted {
                    dealUuid, next_positions_first_turn[i]}));
        engine.subscribeToTurnStarted(observer);
        playCard(*deal, player, 0, i == 0, i == players.size() - 1, 0);
        updateExpectedStateAfterPlay(player);
        assertHandsVisible(*deal, engine.getPlayer(Positions::WEST));
    }

    expectedState.positionInTurn = Positions::NORTH;
    addTrickToNorthSouth();
    std::array<Position, N_PLAYERS> next_positions {
        Positions::EAST, Positions::SOUTH, Positions::EAST, Positions::NORTH
    };
    for (const auto i : from_to(1, N_CARDS_PER_PLAYER)) {
        for (const auto t : boost::combine(players, next_positions)) {
            auto& player = t.get<0>();
            const auto next_position = t.get<1>();
            const auto last_card_in_trick = &player == &players.back();
            const auto turn_notify_count =
                (i == N_CARDS_PER_PLAYER - 1 && last_card_in_trick) ?
                0 : 1;
            const auto deal_notify_count = 1 - turn_notify_count;
            auto turn_observer = std::make_shared<
                StrictMock<MockObserver<BridgeEngine::TurnStarted>>>();
            EXPECT_CALL(
                *turn_observer,
                handleNotify(
                    BridgeEngine::TurnStarted {dealUuid, next_position}))
                .Times(turn_notify_count);
            auto deal_observer =
                std::make_shared<
                    StrictMock<MockObserver<BridgeEngine::DealEnded>>>();
            EXPECT_CALL(*deal_observer, handleNotify(_))
                .Times(deal_notify_count);
            assertDealState(Positions::WEST);
            assertHandsVisible(*deal, engine.getPlayer(Positions::WEST));

            engine.subscribeToTurnStarted(turn_observer);
            engine.subscribeToDealEnded(deal_observer);
            playCard(*deal, *player, i, false, last_card_in_trick, i);
            updateExpectedStateAfterPlay(*player);
        }
        addTrickToNorthSouth();
    }
}

TEST_F(BridgeEngineTest, testPassOut)
{
    setupDependencies();
    startDeal();

    // Two deals as game does not end
    EXPECT_CALL(*cardManager, handleRequestShuffle());
    EXPECT_CALL(*gameManager, handleAddPassedOut());

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    for (const auto& player : players) {
        auto observer = std::make_shared<
            StrictMock<MockObserver<BridgeEngine::BiddingCompleted>>>();
        engine.subscribeToBiddingCompleted(observer);
        EXPECT_CALL(*observer, handleNotify(_)).Times(0);
        engine.call(*player, Pass {});
        engine.startDeal();
    }
    EXPECT_FALSE(engine.hasEnded());
}

TEST_F(BridgeEngineTest, testEndGame)
{
    setupDependencies();
    startDeal();

    // Just one deal as game ends
    EXPECT_CALL(*cardManager, handleRequestShuffle()).Times(0);
    ON_CALL(*gameManager, handleHasEnded()).WillByDefault(Return(false));

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);

    Mock::VerifyAndClearExpectations(cardManager.get());
    ON_CALL(*gameManager, handleHasEnded()).WillByDefault(Return(true));

    for (const auto& player : players) {
        engine.call(*player, Pass {});
        engine.startDeal();
    }
    EXPECT_TRUE(engine.hasEnded());
}

TEST_F(BridgeEngineTest, testSuccessfulCall)
{
    setupDependencies();
    startDeal();

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    auto observer = std::make_shared<
        StrictMock<MockObserver<BridgeEngine::CallMade>>>();
    const auto& player = *players.front();
    const auto uuid = dereference(engine.getCurrentDeal()).getUuid();
    const auto call = Bid {1, Strains::CLUBS};
    EXPECT_CALL(
        *observer,
        handleNotify(BridgeEngine::CallMade {uuid, Positions::NORTH, call, 0}));
    engine.subscribeToCallMade(observer);
    EXPECT_TRUE(engine.call(player, call));
}

TEST_F(BridgeEngineTest, testFailedCall)
{
    setupDependencies();
    startDeal();

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    auto observer = std::make_shared<
        StrictMock<MockObserver<BridgeEngine::CallMade>>>();
    const auto& player = *players.back();
    const auto call = Bid {1, Strains::CLUBS};
    EXPECT_CALL(*observer, handleNotify(_)).Times(0);
    engine.subscribeToCallMade(observer);
    EXPECT_FALSE(engine.call(player, call));
}

TEST_F(BridgeEngineTest, testSuccessfulPlay)
{
    setupDependencies();
    startDeal();

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    engine.call(*players[0], Bid {1, Strains::CLUBS});
    engine.call(*players[1], Pass {});
    engine.call(*players[2], Pass {});
    engine.call(*players[3], Pass {});

    auto observer = std::make_shared<
        StrictMock<MockObserver<BridgeEngine::CardPlayed>>>();
    const auto uuid = dereference(engine.getCurrentDeal()).getUuid();
    const auto& hand = dereference(engine.getHandInTurn());
    EXPECT_CALL(
        *observer,
        handleNotify(
            BridgeEngine::CardPlayed {
                uuid, Positions::EAST, dereference(hand.getCard(0)), 0, 0}));
    engine.subscribeToCardPlayed(observer);
    ASSERT_TRUE(
        engine.play(*players[1], hand, 0));
}

TEST_F(BridgeEngineTest, testFailedPlay)
{
    setupDependencies();
    startDeal();

    shuffledNotifier.notifyAll(Engine::CardManager::ShufflingState::COMPLETED);
    engine.call(*players[0], Bid {1, Strains::CLUBS});
    engine.call(*players[1], Pass {});
    engine.call(*players[2], Pass {});
    engine.call(*players[3], Pass {});
    auto observer = std::make_shared<
        StrictMock<MockObserver<BridgeEngine::CardPlayed>>>();
    EXPECT_CALL(*observer, handleNotify(_)).Times(0);
    engine.subscribeToCardPlayed(observer);
    ASSERT_FALSE(
        engine.play(*players[2], dereference(engine.getHandInTurn()), 0));
}

TEST_F(BridgeEngineTest, testReplacePlayer)
{
    setupDependencies();
    EXPECT_TRUE(engine.setPlayer(Positions::NORTH, nullptr));
    EXPECT_EQ(nullptr, engine.getPlayer(Positions::NORTH));
    EXPECT_TRUE(engine.setPlayer(Positions::NORTH, players[0]));
    EXPECT_EQ(players[0].get(), engine.getPlayer(Positions::NORTH));
}

TEST_F(BridgeEngineTest, testPlayerCannotHaveTwoSeats)
{
    setupDependencies();
    EXPECT_FALSE(engine.setPlayer(Positions::NORTH, players[1]));
    EXPECT_EQ(players[0].get(), engine.getPlayer(Positions::NORTH));
}

TEST_F(BridgeEngineTest, testRecallDealPlayingPhase)
{
    setupDependencies();
    setupRecalledDeal();
    BridgeEngine engine {
        cardManager, gameManager, std::move(recalledDeal)};
    engine.startDeal();

    // Verify the bidding is same as in the recalled deal
    const auto* deal = engine.getCurrentDeal();
    ASSERT_TRUE(deal);
    EXPECT_EQ(UUID, deal->getUuid());
    const auto& bidding = deal->getBidding();
    EXPECT_THAT(
        std::vector(bidding.begin(), bidding.end()),
        ElementsAre(
            std::pair {Positions::NORTH, BID},
            std::pair {Positions::EAST, Pass {}},
            std::pair {Positions::SOUTH, Pass {}},
            std::pair {Positions::WEST, Pass {}}
        ));

    // Verify first trick has the same cards as in the recalled deal
    const auto& trick1 = deal->getTrick(0);
    const auto iter = trick1.begin();
    ASSERT_EQ(4, trick1.end() - iter);
    EXPECT_EQ(&iter->first,     &deal->getHand(Positions::NORTH));
    EXPECT_EQ(&(iter+1)->first, &deal->getHand(Positions::EAST));
    EXPECT_EQ(&(iter+2)->first, &deal->getHand(Positions::SOUTH));
    EXPECT_EQ(&(iter+3)->first, &deal->getHand(Positions::WEST));

    // Verify the second card is empty as in the deal
    const auto& trick2 = deal->getTrick(1);
    EXPECT_EQ(
        &deal->getHand(Positions::EAST),
        &trick2.getLeader());
    EXPECT_EQ(trick2.end(), trick2.begin());
}

TEST_F(BridgeEngineTest, testRecallDealBiddingPhase)
{
    setupDependencies();
    setupRecalledDeal();
    BridgeEngine engine {
        cardManager, gameManager, std::move(recalledDeal)};

    // Ignore west's call, so the bidding is still ongoing
    ON_CALL(bidding_in_deal, handleGetNumberOfCalls())
        .WillByDefault(Return(calls_in_deal.size() - 1));

    engine.startDeal();

    // Verify the bidding is same as in the recalled deal
    const auto* deal = engine.getCurrentDeal();
    ASSERT_TRUE(deal);

    // Verify calls are recalled
    const auto& bidding = deal->getBidding();
    EXPECT_EQ(3, bidding.getNumberOfCalls());
    EXPECT_EQ(Positions::WEST, bidding.getPositionInTurn());

    // Verify there are no tricks recalled
    EXPECT_EQ(0, deal->getNumberOfTricks());
}

TEST_F(BridgeEngineTest, testRecallDealBiddingPhaseFailure)
{
    setupDependencies();
    setupRecalledDeal();
    BridgeEngine engine {
        cardManager, gameManager, std::move(recalledDeal)};

    // Illegal bid
    calls_in_deal[1] = BID;
    EXPECT_THROW(engine.startDeal(), Engine::BridgeEngineFailure);
}

TEST_F(BridgeEngineTest, testRecallDealPlayingPhaseFailure)
{
    setupDependencies();
    setupRecalledDeal();
    BridgeEngine engine {
        cardManager, gameManager, std::move(recalledDeal)};

    // Illegal play out of turn
    ON_CALL(tricks_in_deal[0], handleGetHand(0))
        .WillByDefault(ReturnRef(*hands_in_deal.at(Positions::EAST)));
    ON_CALL(tricks_in_deal[0], handleGetCard(0))
        .WillByDefault(ReturnRef(cards_in_trick[1]));
    EXPECT_THROW(engine.startDeal(), Engine::BridgeEngineFailure);
}
