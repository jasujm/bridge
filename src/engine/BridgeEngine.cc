#include "engine/BridgeEngine.hh"

#include "bridge/BasicBidding.hh"
#include "bridge/BasicTrick.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "engine/CardManager.hh"
#include "engine/GameManager.hh"
#include "FunctionObserver.hh"
#include "FunctionQueue.hh"
#include "Utility.hh"

#include <boost/bimap/bimap.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/range/combine.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/termination.hpp>
#include <boost/statechart/transition.hpp>

#include <algorithm>
#include <vector>

namespace sc = boost::statechart;

namespace Bridge {
namespace Engine {

namespace {

struct PlayInfo {
    PlayInfo(Hand& hand, int card) :
        hand {hand},
        card {card}
    {
    }

    Hand& hand;
    int card;
};

// Helper for making bimap between positions and objects managed by smart
// pointers

template<template<typename...> class PtrType, typename RightType>
auto internalMakePositionMapping(const std::vector<PtrType<RightType>>& values)
{
    using MapType = boost::bimaps::bimap<Position, RightType*>;
    auto func = [](const auto& t) -> typename MapType::value_type
    {
        return {t.template get<0>(), t.template get<1>().get()};
    };
    auto&& z = boost::combine(POSITIONS, values);
    return MapType(
        boost::make_transform_iterator(z.begin(), func),
        boost::make_transform_iterator(z.end(),   func));
}

}

////////////////////////////////////////////////////////////////////////////////
// Events
////////////////////////////////////////////////////////////////////////////////

class StartDealEvent : public sc::event<StartDealEvent> {};
class CallEvent : public sc::event<CallEvent> {
public:
    CallEvent(const Player& player, const Call& call, bool& ret) :
        player {player},
        call {call},
        ret {ret}
    {
    }

    const Player& player;
    Call call;
    bool& ret;
};
class GameEndedEvent : public sc::event<GameEndedEvent> {};
class DealCompletedEvent : public sc::event<DealCompletedEvent> {
public:
    explicit DealCompletedEvent(const TricksWon& tricksWon) :
        tricksWon {tricksWon}
    {
    }
    TricksWon tricksWon;
};
class DealPassedOutEvent : public sc::event<DealPassedOutEvent> {};
class NewTrickEvent : public sc::event<NewTrickEvent> {};
class PlayCardEvent : public sc::event<PlayCardEvent> {
public:
    PlayCardEvent(
        const Player& player, const Hand& hand, int card, bool& ret) :
        player {player},
        hand {hand},
        card {card},
        ret {ret}
    {
    }

    const Player& player;
    const Hand& hand;
    int card;
    bool& ret;
};
class CardRevealEvent : public sc::event<CardRevealEvent> {
public:
    CardRevealEvent(Hand::CardRevealState state, int card) :
        state {state},
        card {card}
    {
    }
    Hand::CardRevealState state;
    int card;
};
class HandRevealEvent : public sc::event<HandRevealEvent> {
public:
    HandRevealEvent(Hand::CardRevealState state) :
        state {state}
    {
    }
    Hand::CardRevealState state;
};
class RevealDummyEvent : public sc::event<RevealDummyEvent> {};
class ShufflingStateEvent : public sc::event<ShufflingStateEvent> {
public:
    explicit ShufflingStateEvent(CardManager::ShufflingState state) :
        state {state}
    {
    }
    CardManager::ShufflingState state;
};

////////////////////////////////////////////////////////////////////////////////
// BridgeEngine::Impl
////////////////////////////////////////////////////////////////////////////////

class Idle;

class BridgeEngine::Impl :
    public sc::state_machine<BridgeEngine::Impl, Idle>,
    public Observer<CardManager::ShufflingState> {
public:
    Impl(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager);

    void setPlayer(Position position, std::shared_ptr<Player> player);

    CardManager& getCardManager() { return dereference(cardManager); }
    GameManager& getGameManager() { return dereference(gameManager); }
    Observable<DealStarted>& getDealStartedNotifier()
    {
        return dealStartedNotifier;
    }
    Observable<TurnStarted>& getTurnStartedNotifier()
    {
        return turnStartedNotifier;
    }
    Observable<CallMade>& getCallMadeNotifier()
    {
        return callMadeNotifier;
    }
    Observable<BiddingCompleted>& getBiddingCompletedNotifier()
    {
        return biddingCompletedNotifier;
    }
    Observable<CardPlayed>& getCardPlayedNotifier()
    {
        return cardPlayedNotifier;
    }
    Observable<TrickCompleted>& getTrickCompletedNotifier()
    {
        return trickCompletedNotifier;
    }
    Observable<DummyRevealed>& getDummyRevealedNotifier()
    {
        return dummyRevealedNotifier;
    }
    Observable<DealEnded>& getDealEndedNotifier()
    {
        return dealEndedNotifier;
    }

    std::optional<Vulnerability> getVulnerability() const;
    std::optional<Position> getPositionInTurn() const;
    const Player* getPlayerInTurn() const;
    const Hand* getHandInTurn() const;
    const Player* getPlayer(Position position) const;
    std::optional<Position> getPosition(const Player& player) const;
    const Hand* getHand(Position position) const;
    std::optional<Position> getPosition(const Hand& hand) const;
    const Bidding* getBidding() const;
    const Trick* getCurrentTrick() const;
    std::optional<int> getNumberOfTricksPlayed() const;
    std::optional<TricksWon> getTricksWon() const;
    const Hand* getDummyHandIfVisible() const;
    auto makeCardRevealStateObserver();

    FunctionQueue functionQueue;

private:

    template<typename T>
    struct InternalCallIfInStateHelper;

    template<typename State, typename R, typename... Args1, typename... Args2>
    auto internalCallIfInState(
        R (State::*const memfn)(Args1...) const,
        Args2&&... args) const;

    void handleNotify(const CardManager::ShufflingState&) override;

    const std::shared_ptr<CardManager> cardManager;
    const std::shared_ptr<GameManager> gameManager;
    std::vector<std::shared_ptr<Player>> players;
    boost::bimaps::bimap<Position, Player*> playersMap;
    std::shared_ptr<Hand> lockedHand;
    Observable<DealStarted> dealStartedNotifier;
    Observable<TurnStarted> turnStartedNotifier;
    Observable<CallMade> callMadeNotifier;
    Observable<BiddingCompleted> biddingCompletedNotifier;
    Observable<CardPlayed> cardPlayedNotifier;
    Observable<TrickCompleted> trickCompletedNotifier;
    Observable<DummyRevealed> dummyRevealedNotifier;
    Observable<DealEnded> dealEndedNotifier;
};

BridgeEngine::Impl::Impl(
    std::shared_ptr<CardManager> cardManager,
    std::shared_ptr<GameManager> gameManager) :
    cardManager {std::move(cardManager)},
    gameManager {std::move(gameManager)}
{
}

auto BridgeEngine::Impl::makeCardRevealStateObserver()
{
    return makeObserver<Hand::CardRevealState, Hand::IndexVector>(
        [this](const auto& state, const auto& ns)
        {
            const auto first = ns.begin();
            const auto last = ns.end();
            const auto n_cards = std::distance(first, last);
            if (n_cards == 1) {
                functionQueue(
                    [this, state, n = *first]()
                    {
                        this->process_event(CardRevealEvent {state, n});
                    });
            } else {
                // All cards are included in the request
                const auto range = to(N_CARDS_PER_PLAYER);
                if (
                    std::is_permutation(
                        range.begin(), range.end(), first, last)) {
                    functionQueue(
                        [this, state]()
                        {
                            this->process_event(HandRevealEvent {state});
                        });
                }
            }
        });
}

// Find other method definitions later (they refer to states)

////////////////////////////////////////////////////////////////////////////////
// Shuffling
////////////////////////////////////////////////////////////////////////////////

class InDeal;

class Shuffling : public sc::state<Shuffling, BridgeEngine::Impl> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<ShufflingStateEvent>,
        sc::termination<GameEndedEvent>>;

    Shuffling(my_context ctx);

    sc::result react(const ShufflingStateEvent&);
};

Shuffling::Shuffling(my_context ctx) :
    my_base {ctx}
{
    auto& context = outermost_context();
    if (context.getGameManager().hasEnded()) {
        post_event(GameEndedEvent {});
    } else {
        context.getCardManager().requestShuffle();
    }
}

sc::result Shuffling::react(const ShufflingStateEvent& event)
{
    if (event.state == CardManager::ShufflingState::COMPLETED) {
        const auto& card_manager = outermost_context().getCardManager();
        if (card_manager.getNumberOfCards() == N_CARDS) {
            return transit<InDeal>();
        }
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// Idle
////////////////////////////////////////////////////////////////////////////////

class Shuffling;

class Idle : public sc::simple_state<Idle, BridgeEngine::Impl> {
public:
    using reactions = sc::transition<StartDealEvent, Shuffling>;
};

////////////////////////////////////////////////////////////////////////////////
// InDeal
////////////////////////////////////////////////////////////////////////////////

class InBidding;

class InDeal : public sc::state<InDeal, BridgeEngine::Impl, InBidding> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<DealCompletedEvent>,
        sc::custom_reaction<DealPassedOutEvent>>;

    InDeal(my_context ctx);

    sc::result react(const DealCompletedEvent&);
    sc::result react(const DealPassedOutEvent&);

    Bidding& getBidding();
    const Bidding& getBidding() const;

    Hand& getHand(Position position);
    const Hand& getHand(Position position) const;
    auto getHands(Position first = Position::NORTH) const;

    Position getPosition(const Hand& player) const;

private:

    using HandsMap = boost::bimaps::bimap<Position, Hand*>;

    auto internalMakeBidding();
    auto internalMakeHands();

    const std::unique_ptr<Bidding> bidding;
    const std::vector<std::shared_ptr<Hand>> hands;
    const HandsMap handsMap;
};

auto InDeal::internalMakeBidding()
{
    const auto& game_manager = outermost_context().getGameManager();
    return std::make_unique<BasicBidding>(
        dereference(game_manager.getOpenerPosition()));
}

auto InDeal::internalMakeHands()
{
    auto& card_manager = outermost_context().getCardManager();
    auto func = [&card_manager](const auto position)
    {
        const auto ns = cardsFor(position);
        return card_manager.getHand(ns.begin(), ns.end());
    };
    return std::vector<std::shared_ptr<Hand>>(
        boost::make_transform_iterator(POSITIONS.begin(), func),
        boost::make_transform_iterator(POSITIONS.end(),   func));
}

InDeal::InDeal(my_context ctx) :
    my_base {ctx},
    bidding {internalMakeBidding()},
    hands {internalMakeHands()},
    handsMap {internalMakePositionMapping(hands)}
{
    auto& context = outermost_context();
    const auto& game_manager = context.getGameManager();
    const auto opener_position = dereference(game_manager.getOpenerPosition());
    context.getDealStartedNotifier().notifyAll(
        BridgeEngine::DealStarted {
            opener_position,
            dereference(game_manager.getVulnerability())});
    context.getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {opener_position});
}

Bidding& InDeal::getBidding()
{
    assert(bidding);
    return *bidding;
}

const Bidding& InDeal::getBidding() const
{
    return const_cast<InDeal&>(*this).getBidding();
}

Hand& InDeal::getHand(const Position position)
{
    return dereference(handsMap.left.at(position));
}

const Hand& InDeal::getHand(const Position position) const
{
    // This const_cast is safe as we are not actually writing to any member
    // variable in non-const version
    return const_cast<InDeal&>(*this).getHand(position);
}

auto InDeal::getHands(const Position first) const
{
    auto positions = POSITIONS;
    const auto position_iter = std::find(
        positions.begin(), positions.end(), first);
    std::rotate(positions.begin(), position_iter, positions.end());
    auto func = [this](const Position position) -> decltype(auto)
    {
        return getHand(position);
    };
    return std::vector<std::reference_wrapper<const Hand>>(
        boost::make_transform_iterator(positions.begin(), func),
        boost::make_transform_iterator(positions.end(), func));
}

Position InDeal::getPosition(const Hand& hand) const
{
    // This const_cast is safe as the hand is only used as key
    return handsMap.right.at(const_cast<Hand*>(&hand));
}

sc::result InDeal::react(const DealCompletedEvent& event)
{
    assert(bidding);
    const auto declarer = bidding->getDeclarerPosition();
    const auto contract = bidding->getContract();
    if (!declarer || !*declarer || !contract || !*contract) {
        return discard_event();
    }


    auto& context = outermost_context();
    const auto declarer_partnership = partnershipFor(**declarer);
    const auto result = context.getGameManager().addResult(
        declarer_partnership, **contract,
        getNumberOfTricksWon(event.tricksWon, declarer_partnership));
    context.getDealEndedNotifier().notifyAll(
        BridgeEngine::DealEnded {event.tricksWon, result});
    return transit<Idle>();
}

sc::result InDeal::react(const DealPassedOutEvent&)
{
    auto& context = outermost_context();
    const auto result = context.getGameManager().addPassedOut();
    context.getDealEndedNotifier().notifyAll(
        BridgeEngine::DealEnded {TricksWon {0, 0}, result});
    return transit<Idle>();
}

////////////////////////////////////////////////////////////////////////////////
// InBidding
////////////////////////////////////////////////////////////////////////////////

class Playing;

class InBidding : public sc::simple_state<InBidding, InDeal> {
public:
    using reactions = sc::custom_reaction<CallEvent>;

    sc::result react(const CallEvent&);
};

sc::result InBidding::react(const CallEvent& event)
{
    auto& bidding = context<InDeal>().getBidding();
    const auto position = outermost_context().getPosition(event.player);
    if (position && bidding.call(*position, event.call)) {
        event.ret = true;
        auto& context = outermost_context();
        context.getCallMadeNotifier().notifyAll(
            BridgeEngine::CallMade { event.player, event.call });
        if (const auto next_position_in_turn = bidding.getPositionInTurn()) {
            context.getTurnStartedNotifier().notifyAll(
                BridgeEngine::TurnStarted(*next_position_in_turn));
        }
        const auto outer_contract = bidding.getContract();
        const auto outer_declarer_position = bidding.getDeclarerPosition();
        if (outer_contract && outer_declarer_position) {
            const auto inner_contract = *outer_contract;
            const auto inner_declarer_position = *outer_declarer_position;
            if (inner_contract && inner_declarer_position) {
                context.getBiddingCompletedNotifier().notifyAll(
                    BridgeEngine::BiddingCompleted {
                        *inner_declarer_position, *inner_contract});
                return transit<Playing>();
            } else {
                post_event(DealPassedOutEvent {});
            }
        }
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// Playing
////////////////////////////////////////////////////////////////////////////////

class PlayingTrick;
class DummyNotVisible;

class Playing : public sc::simple_state<
    Playing, InDeal, boost::mpl::list<PlayingTrick, DummyNotVisible>> {
public:

    void addTrick(std::unique_ptr<Trick>&& trick);
    int getNumberOfTricksPlayed() const;
    TricksWon getTricksWon() const;
    Position getLeaderPosition() const;
    Position getDeclarerPosition() const;
    Position getDummyPosition() const;
    Hand& getDummyHand();
    const Hand& getDummyHand() const;
    std::optional<Suit> getTrump() const;

private:
    std::vector<std::unique_ptr<Trick>> tricks;
};

void Playing::addTrick(std::unique_ptr<Trick>&& trick)
{
    tricks.emplace_back(std::move(trick));
    const auto& added_trick = *tricks.back();
    const auto& winner_hand = dereference(getWinner(added_trick, getTrump()));
    outermost_context().getTrickCompletedNotifier().notifyAll(
        BridgeEngine::TrickCompleted {added_trick, winner_hand});
    if (tricks.size() == N_CARDS_PER_PLAYER) {
        post_event(DealCompletedEvent {getTricksWon()});
    } else {
        post_event(NewTrickEvent {});
    }
}

int Playing::getNumberOfTricksPlayed() const
{
    return tricks.size();
}

TricksWon Playing::getTricksWon() const
{
    auto north_south = 0;
    auto east_west = 0;
    const auto trump = getTrump();

    for (const auto& trick : tricks) {
        assert(trick);
        if (const auto winner = getWinner(*trick, trump)) {
            const auto winner_position = context<InDeal>().getPosition(*winner);
            const auto winner_partnership = partnershipFor(winner_position);
            switch (winner_partnership) {
            case Partnership::NORTH_SOUTH:
                ++north_south;
                break;
            case Partnership::EAST_WEST:
                ++east_west;
            }
        }
    }
    return {north_south, east_west};
}

Position Playing::getLeaderPosition() const
{
    if (tricks.empty()) {
        const auto declarer_position = getDeclarerPosition();
        return clockwise(declarer_position);
    }
    const auto& winner = dereference(getWinner(*tricks.back(), getTrump()));
    return context<InDeal>().getPosition(winner);
}

Position Playing::getDeclarerPosition() const
{
    return dereference(
        dereference(context<InDeal>().getBidding().getDeclarerPosition()));
}

Position Playing::getDummyPosition() const
{
    return partnerFor(getDeclarerPosition());
}

Hand& Playing::getDummyHand()
{
    return context<InDeal>().getHand(getDummyPosition());
}

const Hand& Playing::getDummyHand() const
{
    // This const_cast is safe as we are not actually writing to any member
    // variable in non-const version
    return const_cast<Playing&>(*this).getDummyHand();
}

std::optional<Suit> Playing::getTrump() const
{
    const auto contract =
        dereference(dereference(context<InDeal>().getBidding().getContract()));
    switch (contract.bid.strain) {
    case Strain::CLUBS:
        return Suit::CLUBS;
    case Strain::DIAMONDS:
        return Suit::DIAMONDS;
    case Strain::HEARTS:
        return Suit::HEARTS;
    case Strain::SPADES:
        return Suit::SPADES;
    default:
        return std::nullopt;
    }
}

////////////////////////////////////////////////////////////////////////////////
// PlayingTrick
////////////////////////////////////////////////////////////////////////////////

class PlayingCard;

class PlayingTrick : public sc::state<PlayingTrick, Playing, PlayingCard> {
public:
    using reactions = sc::transition<NewTrickEvent, PlayingTrick>;

    PlayingTrick(my_context ctx);

    void playNext(Hand& hand, int card);
    void play();

    const Trick* getTrick() const;
    std::optional<Position> getPositionInTurn() const;
    const Hand* getHandInTurn() const;
    Hand* getHandIfHasTurn(const Player& player, const Hand& hand);
    const auto& getNextPlay();

private:

    std::optional<PlayInfo> playInfo;
    std::unique_ptr<Trick> trick;
};

PlayingTrick::PlayingTrick(my_context ctx) :
    my_base {ctx}
{
    const auto leader_position = context<Playing>().getLeaderPosition();
    const auto& hands = context<InDeal>().getHands(leader_position);
    trick = std::make_unique<BasicTrick>(hands.begin(), hands.end());
    outermost_context().getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {dereference(getPositionInTurn())});
}

void PlayingTrick::playNext(Hand& hand, int card)
{
    playInfo.emplace(hand, card);
}

class DummyVisible;

void PlayingTrick::play()
{
    assert(playInfo);
    assert(trick);
    const auto& card = dereference(playInfo->hand.getCard(playInfo->card));
    if (trick->play(playInfo->hand, card)) {
        auto& context_ = outermost_context();
        playInfo->hand.markPlayed(playInfo->card);
        context_.getCardPlayedNotifier().notifyAll(
            BridgeEngine::CardPlayed { playInfo->hand, card });
        if (trick->isCompleted()) {
            context<Playing>().addTrick(std::move(trick));
        }
        // If dummy is not yet visible, turn notification is deferred until
        // revealing is completed
        else if (state_cast<const DummyVisible*>()) {
            context_.getTurnStartedNotifier().notifyAll(
                BridgeEngine::TurnStarted {dereference(getPositionInTurn())});
        } else {
            post_event(RevealDummyEvent {});
        }
    }
}

const Trick* PlayingTrick::getTrick() const
{
    return trick.get();
}

std::optional<Position> PlayingTrick::getPositionInTurn() const
{
    if (const auto* hand = getHandInTurn()) {
        auto position = context<InDeal>().getPosition(*hand);
        // Declarer plays for dummy
        if (position == context<Playing>().getDummyPosition()) {
            position = partnerFor(position);
        }
        return position;
    }
    return std::nullopt;
}

const Hand* PlayingTrick::getHandInTurn() const
{
    if (trick) {
        return trick->getHandInTurn();
    }
    return nullptr;
}

Hand* PlayingTrick::getHandIfHasTurn(const Player& player, const Hand& hand)
{
    if (outermost_context().getPosition(player) == getPositionInTurn() &&
        &hand == getHandInTurn()) {
        auto& in_deal = context<InDeal>();
        // Arguably a peculiar sequence of commands but correct in the sense
        // that it converts const Hand& into (non-const) Hand* using the proper
        // interface
        const auto position = in_deal.getPosition(hand);
        return &in_deal.getHand(position);
    }
    return nullptr;
}

const auto& PlayingTrick::getNextPlay()
{
    return playInfo;
}

////////////////////////////////////////////////////////////////////////////////
// PlayingCard
////////////////////////////////////////////////////////////////////////////////

class RevealingCard;

class PlayingCard : public sc::simple_state<PlayingCard, PlayingTrick> {
public:
    using reactions = sc::custom_reaction<PlayCardEvent>;
    sc::result react(const PlayCardEvent&);
};

sc::result PlayingCard::react(const PlayCardEvent& event)
{
    auto& playing_trick = context<PlayingTrick>();
    const auto hand = playing_trick.getHandIfHasTurn(event.player, event.hand);
    if (hand && canBePlayedFromHand(*hand, event.card)) {
        event.ret = true;
        playing_trick.playNext(*hand, event.card);
        return transit<RevealingCard>();
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// RevealingCard
////////////////////////////////////////////////////////////////////////////////

class RevealingCard : public sc::state<RevealingCard, PlayingTrick> {
public:
    using reactions = sc::custom_reaction<CardRevealEvent>;
    RevealingCard(my_context ctx);
    sc::result react(const CardRevealEvent&);

private:
    const std::shared_ptr<Hand::CardRevealStateObserver> observer;
};

RevealingCard::RevealingCard(my_context ctx) :
    my_base {ctx},
    observer {outermost_context().makeCardRevealStateObserver()}
{
    const auto& playInfo = dereference(context<PlayingTrick>().getNextPlay());
    playInfo.hand.subscribe(observer);
    const auto range = { playInfo.card };
    playInfo.hand.requestReveal(range.begin(), range.end());
}

sc::result RevealingCard::react(const CardRevealEvent& event)
{
    const auto& playInfo = dereference(context<PlayingTrick>().getNextPlay());
    if (event.state == Hand::CardRevealState::COMPLETED &&
        event.card == playInfo.card) {
        context<PlayingTrick>().play();
        return transit<PlayingCard>();
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// DummyNotVisible
////////////////////////////////////////////////////////////////////////////////

class RevealingDummy;

class DummyNotVisible : public sc::simple_state<
    DummyNotVisible, Playing::orthogonal<1>> {
public:
    using reactions = sc::transition<RevealDummyEvent, RevealingDummy>;
};

////////////////////////////////////////////////////////////////////////////////
// RevealingDummy
////////////////////////////////////////////////////////////////////////////////

class RevealingDummy : public sc::state<
    RevealingDummy, Playing::orthogonal<1>> {
public:
    using reactions = sc::custom_reaction<HandRevealEvent>;
    RevealingDummy(my_context ctx);
    sc::result react(const HandRevealEvent&);

private:
    const std::shared_ptr<Hand::CardRevealStateObserver> observer;
};

RevealingDummy::RevealingDummy(my_context ctx) :
    my_base {ctx},
    observer {outermost_context().makeCardRevealStateObserver()}
{
    auto& hand = context<Playing>().getDummyHand();
    hand.subscribe(observer);
    requestRevealHand(hand);
}

sc::result RevealingDummy::react(const HandRevealEvent& event)
{
    if (event.state == Hand::CardRevealState::COMPLETED) {
        return transit<DummyVisible>();
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// DummyVisible
////////////////////////////////////////////////////////////////////////////////

class DummyVisible : public sc::state<DummyVisible, Playing::orthogonal<1>> {
public:
    DummyVisible(my_context ctx);
    const Hand& getDummyHand() const;
};

DummyVisible::DummyVisible(my_context ctx) :
    my_base {ctx}
{
    auto& context_ = outermost_context();
    context_.getDummyRevealedNotifier().notifyAll(
        BridgeEngine::DummyRevealed {});
    context_.getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {context<Playing>().getDeclarerPosition()});
}

const Hand& DummyVisible::getDummyHand() const
{
    return context<Playing>().getDummyHand();
}

////////////////////////////////////////////////////////////////////////////////
// BridgeEngine::Impl (method definitions)
////////////////////////////////////////////////////////////////////////////////

// The following defines a helper for conditionally returning a value from a
// method call if the BridgeEngine::Impl is in a given state. Return values of
// methods returning by value are wrapped into std::optional and return
// values of methods returning by reference are returned as pointers. The
// default values if the engine is not in the given state are then none and
// nullptr, respectively.

template<typename T>
struct BridgeEngine::Impl::InternalCallIfInStateHelper
{
    using ReturnType = std::optional<T>;
    static ReturnType returnValue(T&& v) { return std::move(v); }
};

template<typename T>
struct BridgeEngine::Impl::InternalCallIfInStateHelper<T&>
{
    using ReturnType = T*;
    static ReturnType returnValue(T& v) { return &v; }
};

template<typename State, typename R, typename... Args1, typename... Args2>
auto BridgeEngine::Impl::internalCallIfInState(
    R (State::*const memfn)(Args1...) const,
    Args2&&... args) const
{
    using Helper = InternalCallIfInStateHelper<R>;
    if (const auto* state = state_cast<const State*>()) {
        return Helper::returnValue(
            (state->*memfn)(std::forward<Args2>(args)...));
    }
    return typename Helper::ReturnType {};
}

void BridgeEngine::Impl::setPlayer(
    const Position position, std::shared_ptr<Player> player)
{
    players.push_back(std::move(player));
    playersMap.insert({position, players.back().get()});
}

std::optional<Vulnerability> BridgeEngine::Impl::getVulnerability() const
{
    return dereference(gameManager).getVulnerability();
}

std::optional<Position> BridgeEngine::Impl::getPositionInTurn() const
{
    if (state_cast<const InBidding*>()) {
        const auto& bidding = state_cast<const InDeal&>().getBidding();
        return bidding.getPositionInTurn();
    } else if (const auto* state = state_cast<const PlayingTrick*>()) {
        return state->getPositionInTurn();
    }
    return std::nullopt;
}

const Player* BridgeEngine::Impl::getPlayerInTurn() const
{
    const auto position = getPositionInTurn();
    if (position) {
        return getPlayer(*position);
    }
    return nullptr;
}

const Hand* BridgeEngine::Impl::getHandInTurn() const
{
    if (const auto* state = state_cast<const PlayingTrick*>()) {
        return state->getHandInTurn();
    }
    return nullptr;
}

const Player* BridgeEngine::Impl::getPlayer(const Position position) const
{
    const auto iter = playersMap.left.find(position);
    return (iter != playersMap.left.end()) ? iter->second : nullptr;
}

std::optional<Position> BridgeEngine::Impl::getPosition(
    const Player& player) const
{
    // This const_cast is safe as the player is only used as key
    const auto iter = playersMap.right.find(const_cast<Player*>(&player));
    if (iter != playersMap.right.end()) {
        return iter->second;
    }
    return std::nullopt;
}

const Hand* BridgeEngine::Impl::getHand(const Position position) const
{
    return internalCallIfInState(&InDeal::getHand, position);
}

std::optional<Position> BridgeEngine::Impl::getPosition(
    const Hand& hand) const
{
    return internalCallIfInState(&InDeal::getPosition, hand);
}

const Bidding* BridgeEngine::Impl::getBidding() const
{
    return internalCallIfInState(&InDeal::getBidding);
}

const Trick* BridgeEngine::Impl::getCurrentTrick() const
{
    if (const auto* state = state_cast<const PlayingTrick*>()) {
        return state->getTrick();
    }
    return nullptr;
}

std::optional<int> BridgeEngine::Impl::getNumberOfTricksPlayed() const
{
    return internalCallIfInState(&Playing::getNumberOfTricksPlayed);
}

std::optional<TricksWon> BridgeEngine::Impl::getTricksWon() const
{
    return internalCallIfInState(&Playing::getTricksWon);
}

const Hand* BridgeEngine::Impl::getDummyHandIfVisible() const
{
    return internalCallIfInState(&DummyVisible::getDummyHand);
}

void BridgeEngine::Impl::handleNotify(const CardManager::ShufflingState& state)
{
    functionQueue(
        [this, state]()
        {
            process_event(ShufflingStateEvent {state});
        });
}

////////////////////////////////////////////////////////////////////////////////
// BridgeEngine
////////////////////////////////////////////////////////////////////////////////

BridgeEngine::BridgeEngine(
    std::shared_ptr<CardManager> cardManager,
    std::shared_ptr<GameManager> gameManager) :
    impl {
        std::make_shared<Impl>(cardManager, std::move(gameManager))}
{
    dereference(cardManager).subscribe(impl);
    impl->initiate();
}

BridgeEngine::~BridgeEngine() = default;

void BridgeEngine::subscribeToDealStarted(
    std::weak_ptr<Observer<DealStarted>> observer)
{
    assert(impl);
    impl->getDealStartedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToTurnStarted(
    std::weak_ptr<Observer<TurnStarted>> observer)
{
    assert(impl);
    impl->getTurnStartedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToCallMade(
    std::weak_ptr<Observer<CallMade>> observer)
{
    assert(impl);
    impl->getCallMadeNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToBiddingCompleted(
    std::weak_ptr<Observer<BiddingCompleted>> observer)
{
    assert(impl);
    impl->getBiddingCompletedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToCardPlayed(
    std::weak_ptr<Observer<CardPlayed>> observer)
{
    assert(impl);
    impl->getCardPlayedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToTrickCompleted(
    std::weak_ptr<Observer<TrickCompleted>> observer)
{
    assert(impl);
    impl->getTrickCompletedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToDummyRevealed(
    std::weak_ptr<Observer<DummyRevealed>> observer)
{
    assert(impl);
    impl->getDummyRevealedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::subscribeToDealEnded(
    std::weak_ptr<Observer<DealEnded>> observer)
{
    assert(impl);
    impl->getDealEndedNotifier().subscribe(std::move(observer));
}

void BridgeEngine::startDeal()
{
    assert(impl);
    impl->functionQueue(
        [&impl = *impl]()
        {
            impl.process_event(StartDealEvent {});
        });
}

void BridgeEngine::setPlayer(
    const Position position, std::shared_ptr<Player> player)
{
    assert(impl);
    impl->setPlayer(position, std::move(player));
}

bool BridgeEngine::call(const Player& player, const Call& call)
{
    assert(impl);
    auto ret = false;
    impl->functionQueue(
        [&impl = *impl, &player, &call, &ret]()
        {
            impl.process_event(CallEvent {player, call, ret});
        });
    return ret;
}

bool BridgeEngine::play(
    const Player& player, const Hand& hand, int card)
{
    assert(impl);
    auto ret = false;
    impl->functionQueue(
        [&impl = *impl, &player, &hand, card, &ret]()
        {
            impl.process_event(PlayCardEvent {player, hand, card, ret});
        });
    return ret;
}

bool BridgeEngine::hasEnded() const
{
    return impl->terminated();
}

std::optional<Vulnerability> BridgeEngine::getVulnerability() const
{
    assert(impl);
    return impl->getVulnerability();
}

const Player* BridgeEngine::getPlayerInTurn() const
{
    assert(impl);
    return impl->getPlayerInTurn();
}

std::optional<Position> BridgeEngine::getPositionInTurn() const
{
    assert(impl);
    return impl->getPositionInTurn();
}

const Hand* BridgeEngine::getHandInTurn() const
{
    assert(impl);
    return impl->getHandInTurn();
}

const Player* BridgeEngine::getPlayer(const Position position) const
{
    assert(impl);
    return impl->getPlayer(position);
}

bool BridgeEngine::isVisible(const Hand& hand, const Player& player) const
{
    assert(impl);
    const auto position = getPosition(player);
    return (position && &hand == getHand(*position)) ||
        &hand == impl->getDummyHandIfVisible();
}

std::optional<Position> BridgeEngine::getPosition(const Player& player) const
{
    assert(impl);
    return impl->getPosition(player);
}

const Hand* BridgeEngine::getHand(const Position position) const
{
    assert(impl);
    return impl->getHand(position);
}

std::optional<Position> BridgeEngine::getPosition(const Hand& hand) const
{
    assert(impl);
    return impl->getPosition(hand);
}

const Bidding* BridgeEngine::getBidding() const
{
    assert(impl);
    return impl->getBidding();
}

const Trick* BridgeEngine::getCurrentTrick() const
{
    assert(impl);
    return impl->getCurrentTrick();
}

std::optional<int> BridgeEngine::getNumberOfTricksPlayed() const
{
    assert(impl);
    return impl->getNumberOfTricksPlayed();
}

std::optional<TricksWon> BridgeEngine::getTricksWon() const
{
    assert(impl);
    return impl->getTricksWon();
}

BridgeEngine::DealStarted::DealStarted(
    const Position opener, const Vulnerability vulnerability) :
    opener {opener},
    vulnerability {vulnerability}
{
}

BridgeEngine::TurnStarted::TurnStarted(const Position position) :
    position {position}
{
}

BridgeEngine::CallMade::CallMade(
    const Player& player, const Call& call) :
    player {player},
    call {call}
{
}

BridgeEngine::BiddingCompleted::BiddingCompleted(
    const Position declarer, const Contract& contract) :
    declarer {declarer},
    contract {contract}
{
}

BridgeEngine::CardPlayed::CardPlayed(const Hand& hand, const Card& card) :
    hand {hand},
    card {card}
{
}

BridgeEngine::TrickCompleted::TrickCompleted(
    const Trick& trick, const Hand& winner) :
    trick {trick},
    winner {winner}
{
}

BridgeEngine::DealEnded::DealEnded(
    const TricksWon& tricksWon, const std::any& result) :
    tricksWon {tricksWon},
    result {result}
{
}

bool operator==(
    const BridgeEngine::DealStarted& lhs, const BridgeEngine::DealStarted& rhs)
{
    return lhs.opener == rhs.opener && lhs.vulnerability == rhs.vulnerability;
}

bool operator==(
    const BridgeEngine::TurnStarted& lhs, const BridgeEngine::TurnStarted& rhs)
{
    return lhs.position == rhs.position;
}

bool operator==(
    const BridgeEngine::CallMade& lhs, const BridgeEngine::CallMade& rhs)
{
    return &lhs.player == &rhs.player &&
        lhs.call == rhs.call;
}
bool operator==(
    const BridgeEngine::BiddingCompleted& lhs,
    const BridgeEngine::BiddingCompleted& rhs)
{
    return lhs.declarer == rhs.declarer && lhs.contract == rhs.contract;
}

bool operator==(
    const BridgeEngine::CardPlayed& lhs, const BridgeEngine::CardPlayed& rhs)
{
    return &lhs.hand == &rhs.hand &&
        &lhs.card == &rhs.card;
}

bool operator==(
    const BridgeEngine::TrickCompleted& lhs,
    const BridgeEngine::TrickCompleted& rhs)
{
    return &lhs.trick == &rhs.trick && &lhs.winner == &rhs.winner;
}

bool operator==(
    const BridgeEngine::DealEnded& lhs, const BridgeEngine::DealEnded& rhs)
{
    // TODO: Also results should compare equal
    return lhs.tricksWon == rhs.tricksWon;
}

}
}
