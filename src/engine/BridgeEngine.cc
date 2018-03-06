#include "engine/BridgeEngine.hh"

#include "bridge/BasicBidding.hh"
#include "bridge/BasicTrick.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "engine/CardManager.hh"
#include "engine/GameManager.hh"
#include "FunctionObserver.hh"
#include "FunctionQueue.hh"
#include "Utility.hh"

#include <boost/bimap/bimap.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>
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
    PlayInfo(Hand& hand, std::size_t card) :
        hand {hand},
        card {card}
    {
    }

    Hand& hand;
    std::size_t card;
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
        const Player& player, const Hand& hand, std::size_t card, bool& ret) :
        player {player},
        hand {hand},
        card {card},
        ret {ret}
    {
    }

    const Player& player;
    const Hand& hand;
    std::size_t card;
    bool& ret;
};
class CardRevealEvent : public sc::event<CardRevealEvent> {
public:
    CardRevealEvent(Hand::CardRevealState state, std::size_t card) :
        state {state},
        card {card}
    {
    }
    Hand::CardRevealState state;
    std::size_t card;
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

class InDeal;
class Shuffling;

class BridgeEngine::Impl :
    public sc::state_machine<BridgeEngine::Impl, Shuffling>,
    public Observer<CardManager::ShufflingState> {
public:
    Impl(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager);

    void setPlayer(Position position, std::shared_ptr<Player> player);

    void lockHand(std::shared_ptr<Hand> hand);

    CardManager& getCardManager() { return dereference(cardManager); }
    GameManager& getGameManager() { return dereference(gameManager); }
    Observable<ShufflingCompleted>& getShufflingCompletedNotifier()
    {
        return shufflingCompletedNotifier;
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

    boost::optional<Vulnerability> getVulnerability() const;
    boost::optional<Position> getPositionInTurn() const;
    const Player* getPlayerInTurn() const;
    const Hand* getHandInTurn() const;
    const Player* getPlayer(Position position) const;
    boost::optional<Position> getPosition(const Player& player) const;
    const Hand* getHand(Position position) const;
    boost::optional<Position> getPosition(const Hand& hand) const;
    const Bidding* getBidding() const;
    const Trick* getCurrentTrick() const;
    boost::optional<std::size_t> getNumberOfTricksPlayed() const;
    boost::optional<TricksWon> getTricksWon() const;
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
    Observable<ShufflingCompleted> shufflingCompletedNotifier;
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
// InDeal
////////////////////////////////////////////////////////////////////////////////

class InBidding;

class InDeal : public sc::state<InDeal, BridgeEngine::Impl, InBidding> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<DealCompletedEvent>,
        sc::custom_reaction<DealPassedOutEvent>>;

    InDeal(my_context ctx);

    void exit();

    sc::result react(const DealCompletedEvent&);
    sc::result react(const DealPassedOutEvent&);

    void lockHand(const Hand& hand);

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

    template<typename... Args1, typename... Args2>
    sc::result internalCallAndTransit(
        void (GameManager::*memfn)(Args1...),
        Args2&&... args);

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
    outermost_context().getShufflingCompletedNotifier().notifyAll(
        BridgeEngine::ShufflingCompleted {});
}

void InDeal::exit()
{
    // This must be in exit() instead of ~InDeal(). Context might be
    // destructed if we ended up here due to destruction of Impl.
    outermost_context().getDealEndedNotifier().notifyAll(
        BridgeEngine::DealEnded {});
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

    const auto declarer_partnership = partnershipFor(**declarer);
    return internalCallAndTransit(
        &GameManager::addResult,
        declarer_partnership,
        **contract,
        getNumberOfTricksWon(event.tricksWon, declarer_partnership));
}

sc::result InDeal::react(const DealPassedOutEvent&)
{
    return internalCallAndTransit(&GameManager::addPassedOut);
}

void InDeal::lockHand(const Hand& hand)
{
    const auto iter = std::find_if(
        hands.begin(), hands.end(),
        [&hand](const auto& hand_ptr) { return hand_ptr.get() == &hand; });
    assert(iter != hands.end());
    outermost_context().lockHand(std::move(*iter));
}

template<typename... Args1, typename... Args2>
sc::result InDeal::internalCallAndTransit(
    void (GameManager::*const memfn)(Args1...),
    Args2&&... args)
{
    auto& game_manager = outermost_context().getGameManager();
    (game_manager.*memfn)(std::forward<Args2>(args)...);
    return transit<Shuffling>();
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
        const auto has_contract = bidding.hasContract();
        if (has_contract) {
            context.getBiddingCompletedNotifier().notifyAll(
                BridgeEngine::BiddingCompleted {});
            return transit<Playing>();
        } else if (!has_contract) {
            post_event(DealPassedOutEvent {});
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
    std::size_t getNumberOfTricksPlayed() const;
    TricksWon getTricksWon() const;
    Position getLeaderPosition() const;
    Position getDeclarerPosition() const;
    Position getDummyPosition() const;
    Hand& getDummyHand();
    const Hand& getDummyHand() const;
    boost::optional<Suit> getTrump() const;

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

std::size_t Playing::getNumberOfTricksPlayed() const
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

boost::optional<Suit> Playing::getTrump() const
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
        return boost::none;
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

    void playNext(Hand& hand, std::size_t card);
    void play();

    const Trick& getTrick() const;
    Position getPositionInTurn() const;
    const Hand& getHandInTurn() const;
    Hand* getHandIfHasTurn(const Player& player, const Hand& hand);
    const auto& getNextPlay();

private:

    boost::optional<PlayInfo> playInfo;
    std::unique_ptr<Trick> trick;
};

PlayingTrick::PlayingTrick(my_context ctx) :
    my_base {ctx}
{
    const auto& hands = context<InDeal>().getHands(
        context<Playing>().getLeaderPosition());
    trick = std::make_unique<BasicTrick>(hands.begin(), hands.end());
}

void PlayingTrick::playNext(Hand& hand, std::size_t card)
{
    playInfo.emplace(hand, card);
}

void PlayingTrick::play()
{
    assert(playInfo);
    assert(trick);
    const auto& card = dereference(playInfo->hand.getCard(playInfo->card));
    if (trick->play(playInfo->hand, card)) {
        playInfo->hand.markPlayed(playInfo->card);
        outermost_context().getCardPlayedNotifier().notifyAll(
            BridgeEngine::CardPlayed { playInfo->hand, card });
        if (trick->isCompleted()) {
            context<Playing>().addTrick(std::move(trick));
        }
        post_event(RevealDummyEvent {});
    }
}

const Trick& PlayingTrick::getTrick() const
{
    assert(trick);
    return *trick;
}

Position PlayingTrick::getPositionInTurn() const
{
    const auto& hand = getHandInTurn();
    auto position = context<InDeal>().getPosition(hand);
    // Declarer plays for dummy
    if (position == context<Playing>().getDummyPosition()) {
        position = partnerFor(position);
    }
    return position;
}

const Hand& PlayingTrick::getHandInTurn() const
{
    assert(trick);
    return dereference(trick->getHandInTurn());
}

Hand* PlayingTrick::getHandIfHasTurn(const Player& player, const Hand& hand)
{
    if (outermost_context().getPosition(player) == getPositionInTurn() &&
        &hand == &getHandInTurn()) {
        auto& in_deal = context<InDeal>();
        const auto position = in_deal.getPosition(hand);
        auto& hand = in_deal.getHand(position);
        in_deal.lockHand(hand);
        return &hand;
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
    if (hand && hand->getCard(event.card)) {
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


class DummyVisible;

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
    outermost_context().getDummyRevealedNotifier().notifyAll(
        BridgeEngine::DummyRevealed {});
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
// methods returning by value are wrapped into boost::optional and return
// values of methods returning by reference are returned as pointers. The
// default values if the engine is not in the given state are then none and
// nullptr, respectively.

template<typename T>
struct BridgeEngine::Impl::InternalCallIfInStateHelper
{
    using ReturnType = boost::optional<T>;
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

boost::optional<Vulnerability> BridgeEngine::Impl::getVulnerability() const
{
    return dereference(gameManager).getVulnerability();
}

boost::optional<Position> BridgeEngine::Impl::getPositionInTurn() const
{
    if (state_cast<const InBidding*>()) {
        const auto& bidding = state_cast<const InDeal&>().getBidding();
        return bidding.getPositionInTurn();
    } else if (const auto* state = state_cast<const PlayingTrick*>()) {
        return state->getPositionInTurn();
    }
    return boost::none;
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
    return internalCallIfInState(&PlayingTrick::getHandInTurn);
}

const Player* BridgeEngine::Impl::getPlayer(const Position position) const
{
    const auto iter = playersMap.left.find(position);
    return (iter != playersMap.left.end()) ? iter->second : nullptr;
}

boost::optional<Position> BridgeEngine::Impl::getPosition(
    const Player& player) const
{
    // This const_cast is safe as the player is only used as key
    const auto iter = playersMap.right.find(const_cast<Player*>(&player));
    if (iter != playersMap.right.end()) {
        return iter->second;
    }
    return boost::none;
}

const Hand* BridgeEngine::Impl::getHand(const Position position) const
{
    return internalCallIfInState(&InDeal::getHand, position);
}

boost::optional<Position> BridgeEngine::Impl::getPosition(
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
    return internalCallIfInState(&PlayingTrick::getTrick);
}

boost::optional<std::size_t> BridgeEngine::Impl::getNumberOfTricksPlayed() const
{
    return internalCallIfInState(&Playing::getNumberOfTricksPlayed);
}

boost::optional<TricksWon> BridgeEngine::Impl::getTricksWon() const
{
    return internalCallIfInState(&Playing::getTricksWon);
}

const Hand* BridgeEngine::Impl::getDummyHandIfVisible() const
{
    return internalCallIfInState(&DummyVisible::getDummyHand);
}

void BridgeEngine::Impl::lockHand(std::shared_ptr<Hand> hand)
{
    lockedHand.swap(hand);
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
        std::make_shared<Impl>(std::move(cardManager), std::move(gameManager))}
{
}

BridgeEngine::~BridgeEngine() = default;

void BridgeEngine::subscribeToShufflingCompleted(
    std::weak_ptr<Observer<ShufflingCompleted>> observer)
{
    assert(impl);
    impl->getShufflingCompletedNotifier().subscribe(std::move(observer));
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

void BridgeEngine::initiate()
{
    assert(impl);
    impl->getCardManager().subscribe(impl);
    impl->functionQueue(
        [&impl = *impl]()
        {
            impl.initiate();
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
    const Player& player, const Hand& hand, std::size_t card)
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

boost::optional<Vulnerability> BridgeEngine::getVulnerability() const
{
    assert(impl);
    return impl->getVulnerability();
}

const Player* BridgeEngine::getPlayerInTurn() const
{
    assert(impl);
    return impl->getPlayerInTurn();
}

boost::optional<Position> BridgeEngine::getPositionInTurn() const
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

boost::optional<Position> BridgeEngine::getPosition(const Player& player) const
{
    assert(impl);
    return impl->getPosition(player);
}

const Hand* BridgeEngine::getHand(const Position position) const
{
    assert(impl);
    return impl->getHand(position);
}

boost::optional<Position> BridgeEngine::getPosition(const Hand& hand) const
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

boost::optional<std::size_t> BridgeEngine::getNumberOfTricksPlayed() const
{
    assert(impl);
    return impl->getNumberOfTricksPlayed();
}

boost::optional<TricksWon> BridgeEngine::getTricksWon() const
{
    assert(impl);
    return impl->getTricksWon();
}

BridgeEngine::CallMade::CallMade(
    const Player& player, const Call& call) :
    player {player},
    call {call}
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

bool operator==(
    const BridgeEngine::CallMade& lhs, const BridgeEngine::CallMade& rhs)
{
    return &lhs.player == &rhs.player &&
        lhs.call == rhs.call;
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

}
}
