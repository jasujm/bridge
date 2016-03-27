#include "engine/BridgeEngine.hh"

#include "bridge/BasicBidding.hh"
#include "bridge/BasicTrick.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "bridge/TricksWon.hh"
#include "bridge/Vulnerability.hh"
#include "engine/CardManager.hh"
#include "engine/GameManager.hh"
#include "Utility.hh"
#include "Zip.hh"

#include <boost/bimap/bimap.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/termination.hpp>
#include <boost/statechart/transition.hpp>

#include <algorithm>
#include <functional>
#include <queue>

namespace sc = boost::statechart;

namespace Bridge {
namespace Engine {

namespace {

// Helper for making bimap between positions and objects managed by smart
// pointers

template<template<typename...> class PtrType, typename RightType>
auto internalMakePositionMapping(const std::vector<PtrType<RightType>>& values)
{
    using MapType = boost::bimaps::bimap<Position, RightType*>;
    auto func = [](const auto& t)
    {
        return typename MapType::value_type {
            t.template get<0>(), t.template get<1>().get()};
    };
    const auto& z = zip(POSITIONS, values);
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
    const Player& player;
    Call call;
    CallEvent(const Player& player, const Call& call) :
        player {player},
        call {call}
    {
    }
};
class GameEndedEvent : public sc::event<GameEndedEvent> {};
class DealCompletedEvent : public sc::event<DealCompletedEvent> {
public:
    TricksWon tricksWon;
    explicit DealCompletedEvent(const TricksWon& tricksWon) :
        tricksWon {tricksWon}
    {
    }
};
class DealPassedOutEvent : public sc::event<DealPassedOutEvent> {};
class NewTrickEvent : public sc::event<NewTrickEvent> {};
class PlayCardEvent : public sc::event<PlayCardEvent> {
public:
    const Player& player;
    const Hand& hand;
    std::size_t card;
    PlayCardEvent(const Player& player, const Hand& hand, std::size_t card) :
        player {player},
        hand {hand},
        card {card}
    {
    }
};
class RevealDummyEvent : public sc::event<RevealDummyEvent> {};
class ShuffledEvent : public sc::event<ShuffledEvent> {};

////////////////////////////////////////////////////////////////////////////////
// BridgeEngine::Impl
////////////////////////////////////////////////////////////////////////////////

class InDeal;
class Shuffling;

class BridgeEngine::Impl :
    public sc::state_machine<BridgeEngine::Impl, Shuffling>,
    public Observer<Engine::Shuffled>,
    public Observable<DealEnded> {
public:
    using EventQueue = std::queue<std::function<void()>>;

    Impl(
        std::shared_ptr<CardManager> cardManager,
        std::shared_ptr<GameManager> gameManager,
        std::vector<std::shared_ptr<Player>> players);

    using Observable<DealEnded>::subscribe;
    using Observable<DealEnded>::notifyAll;

    void enqueueAndProcess(const EventQueue::value_type event);

    CardManager& getCardManager() { return dereference(cardManager); }
    GameManager& getGameManager() { return dereference(gameManager); }

    boost::optional<Vulnerability> getVulnerability() const;
    const Player* getPlayerInTurn() const;
    const Hand* getHandInTurn() const;
    const Player& getPlayer(Position position) const;
    Position getPosition(const Player& player) const;
    const Hand* getHand(const Player& player) const;
    boost::optional<Position> getPosition(const Hand& hand) const;
    const Bidding* getBidding() const;
    const Trick* getCurrentTrick() const;
    boost::optional<std::size_t> getNumberOfTricksPlayed() const;
    boost::optional<TricksWon> getTricksWon() const;
    const Hand* getDummyHandIfVisible() const;

private:

    template<typename T>
    struct InternalCallIfInStateHelper;

    template<typename State, typename R, typename... Args1, typename... Args2>
    auto internalCallIfInState(
        R (State::*const memfn)(Args1...) const,
        Args2&&... args) const;

    void processQueue();

    void handleNotify(const Engine::Shuffled&) override;

    const std::shared_ptr<CardManager> cardManager;
    const std::shared_ptr<GameManager> gameManager;
    const std::vector<std::shared_ptr<Player>> players;
    const boost::bimaps::bimap<Position, Player*> playersMap;
    EventQueue events;
};

BridgeEngine::Impl::Impl(
    std::shared_ptr<CardManager> cardManager,
    std::shared_ptr<GameManager> gameManager,
    std::vector<std::shared_ptr<Player>> players) :
    cardManager {std::move(cardManager)},
    gameManager {std::move(gameManager)},
    players(std::move(players)),
    playersMap {internalMakePositionMapping(this->players)}
{
}

// Find other method definitions later (they refer to states)

////////////////////////////////////////////////////////////////////////////////
// Shuffling
////////////////////////////////////////////////////////////////////////////////

class Shuffling : public sc::state<Shuffling, BridgeEngine::Impl> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<ShuffledEvent>,
        sc::termination<GameEndedEvent>>;

    Shuffling(my_context ctx);

    sc::result react(const ShuffledEvent&);
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

sc::result Shuffling::react(const ShuffledEvent&)
{
    const auto& card_manager = outermost_context().getCardManager();
    if (card_manager.getNumberOfCards() == N_CARDS) {
        return transit<InDeal>();
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
    const std::vector<std::unique_ptr<Hand>> hands;
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
    return std::vector<std::unique_ptr<Hand>>(
        boost::make_transform_iterator(POSITIONS.begin(), func),
        boost::make_transform_iterator(POSITIONS.end(),   func));
}

InDeal::InDeal(my_context ctx) :
    my_base {ctx},
    bidding {internalMakeBidding()},
    hands {internalMakeHands()},
    handsMap {internalMakePositionMapping(hands)}
{
}

void InDeal::exit()
{
    // This must be in exit() instead of ~InDeal(). Context might be
    // destructed if we ended up here due to destruction of Impl.
    outermost_context().notifyAll(BridgeEngine::DealEnded {});
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
    const auto bidder_position = outermost_context().getPosition(event.player);
    if (bidding.call(bidder_position, event.call)) {
        const auto has_contract = bidding.hasContract();
        if (has_contract) {
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
    boost::optional<Suit> getTrump() const;

private:
    std::vector<std::unique_ptr<Trick>> tricks;
};

void Playing::addTrick(std::unique_ptr<Trick>&& trick)
{
    tricks.emplace_back(std::move(trick));
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

class PlayingTrick : public sc::state<PlayingTrick, Playing> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<PlayCardEvent>,
        sc::transition<NewTrickEvent, PlayingTrick>>;

    PlayingTrick(my_context ctx);

    sc::result react(const PlayCardEvent&);

    const Trick& getTrick() const;
    const Player& getPlayerInTurn() const;
    const Hand& getHandInTurn() const;

private:
    Hand* getHandInTurnFor(const Player& player, const Hand& hand);

    std::unique_ptr<Trick> trick;
};

PlayingTrick::PlayingTrick(my_context ctx) :
    my_base {ctx}
{
    const auto& hands = context<InDeal>().getHands(
        context<Playing>().getLeaderPosition());
    trick = std::make_unique<BasicTrick>(hands.begin(), hands.end());
}

sc::result PlayingTrick::react(const PlayCardEvent& event)
{
    assert(trick);
    if (auto hand = getHandInTurnFor(event.player, event.hand)) {
        const auto& card = hand->getCard(event.card);
        if (card && trick->play(*hand, *card)) {
            hand->markPlayed(event.card);
            if (trick->isCompleted()) {
                context<Playing>().addTrick(std::move(trick));
            }
            post_event(RevealDummyEvent {});
        }
    }
    return discard_event();
}

const Trick& PlayingTrick::getTrick() const
{
    assert(trick);
    return *trick;
}

const Player& PlayingTrick::getPlayerInTurn() const
{
    const auto& hand = getHandInTurn();
    auto position = context<InDeal>().getPosition(hand);
    // Declarer plays for dummy
    if (position == context<Playing>().getDummyPosition()) {
        position = partnerFor(position);
    }
    return outermost_context().getPlayer(position);
}

const Hand& PlayingTrick::getHandInTurn() const
{
    assert(trick);
    return dereference(trick->getHandInTurn());
}

Hand* PlayingTrick::getHandInTurnFor(const Player& player, const Hand& hand)
{
    if (&player == &getPlayerInTurn() && &hand == &getHandInTurn()) {
        auto& in_deal = context<InDeal>();
        const auto position = in_deal.getPosition(hand);
        return &in_deal.getHand(position);
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DummyNotVisible
////////////////////////////////////////////////////////////////////////////////

class DummyVisible;

class DummyNotVisible : public sc::simple_state<
    DummyNotVisible, Playing::orthogonal<1>> {
public:
    using reactions = sc::transition<RevealDummyEvent, DummyVisible>;
};

////////////////////////////////////////////////////////////////////////////////
// DummyNotVisible
////////////////////////////////////////////////////////////////////////////////

class DummyVisible : public sc::simple_state<
    DummyVisible, Playing::orthogonal<1>> {
public:
    const Hand& getDummyHand() const;
};

const Hand& DummyVisible::getDummyHand() const
{
    return context<InDeal>().getHand(context<Playing>().getDummyPosition());
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

boost::optional<Vulnerability> BridgeEngine::Impl::getVulnerability() const
{
    return dereference(gameManager).getVulnerability();
}

const Player* BridgeEngine::Impl::getPlayerInTurn() const
{
    if (state_cast<const InBidding*>()) {
        const auto& bidding = state_cast<const InDeal&>().getBidding();
        return &getPlayer(dereference(bidding.getPositionInTurn()));
    } else if (const auto* state = state_cast<const PlayingTrick*>()) {
        return &state->getPlayerInTurn();
    }
    return nullptr;
}

const Hand* BridgeEngine::Impl::getHandInTurn() const
{
    return internalCallIfInState(&PlayingTrick::getHandInTurn);
}

const Player& BridgeEngine::Impl::getPlayer(const Position position) const
{
    const auto& player = playersMap.left.at(position);
    return dereference(player);
}

Position BridgeEngine::Impl::getPosition(const Player& player) const
{
    // This const_cast is safe as the player is only used as key
    return playersMap.right.at(const_cast<Player*>(&player));
}

const Hand* BridgeEngine::Impl::getHand(const Player& player) const
{
    const auto position = getPosition(player);
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

void BridgeEngine::Impl::enqueueAndProcess(
    const BridgeEngine::Impl::EventQueue::value_type event)
{
    events.emplace(std::move(event));
    if (events.size() == 1) { // if queue was empty
        processQueue();
    }
}

void BridgeEngine::Impl::processQueue()
{
    while (!events.empty()) {
        (events.front())();
        events.pop();
    }
}

void BridgeEngine::Impl::handleNotify(const Engine::Shuffled&)
{
    enqueueAndProcess(
        [this]()
        {
            process_event(ShuffledEvent {});
        });
}

////////////////////////////////////////////////////////////////////////////////
// BridgeEngine
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BridgeEngine::Impl> BridgeEngine::makeImpl(
    std::shared_ptr<CardManager> cardManager,
    std::shared_ptr<GameManager> gameManager,
    std::vector<std::shared_ptr<Player>> players)
{
    auto impl = std::make_shared<Impl>(
        std::move(cardManager),
        std::move(gameManager),
        std::move(players));
    impl->getCardManager().subscribe(impl);
    impl->enqueueAndProcess(
        [&impl = *impl]()
        {
            impl.initiate();
        });
    return impl;
}

BridgeEngine::~BridgeEngine() = default;

void BridgeEngine::subscribe(std::weak_ptr<Observer<DealEnded>> observer)
{
    assert(impl);
    impl->subscribe(std::move(observer));
}

void BridgeEngine::call(const Player& player, const Call& call)
{
    assert(impl);
    impl->enqueueAndProcess(
        [&impl = *impl, &player, &call]()
        {
            impl.process_event(CallEvent {player, call});
        });
}

void BridgeEngine::play(
    const Player& player, const Hand& hand, std::size_t card)
{
    assert(impl);
    impl->enqueueAndProcess(
        [&impl = *impl, &player, &hand, card]()
        {
            impl.process_event(PlayCardEvent {player, hand, card});
        });
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

const Hand* BridgeEngine::getHandInTurn() const
{
    assert(impl);
    return impl->getHandInTurn();
}

const Player& BridgeEngine::getPlayer(const Position position) const
{
    assert(impl);
    return impl->getPlayer(position);
}

Position BridgeEngine::getPosition(const Player& player) const
{
    assert(impl);
    return impl->getPosition(player);
}

const Hand* BridgeEngine::getHand(const Player& player) const
{
    assert(impl);
    return impl->getHand(player);
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

const Hand* BridgeEngine::getDummyHandIfVisible() const
{
    assert(impl);
    return impl->getDummyHandIfVisible();
}

}
}
