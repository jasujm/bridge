#include "engine/BridgeEngine.hh"

#include "bridge/BasicBidding.hh"
#include "bridge/BasicTrick.hh"
#include "bridge/DealResult.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "engine/CardManager.hh"
#include "engine/GameManager.hh"
#include "Utility.hh"
#include "Zip.hh"

#include <boost/logic/tribool.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/optional/optional.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/transition.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

namespace sc = boost::statechart;

namespace Bridge {
namespace Engine {

namespace {

// Helper for forwarding call if engine is in proper state
template<typename State, typename R, typename... Args1, typename... Args2>
boost::optional<R> internalCallIfInState(
    const BridgeEngine& engine,
    R (State::*const memfn)(Args1...) const,
    Args2&&... args)
{
    if (const auto* state = engine.state_cast<const State*>()) {
        return (state->*memfn)(std::forward<Args2>(args)...);
    }
    return boost::none;
}

}

////////////////////////////////////////////////////////////////////////////////
// Internal events
////////////////////////////////////////////////////////////////////////////////

class NewTrickEvent : public sc::event<NewTrickEvent> {};
class DealCompletedEvent : public sc::event<DealCompletedEvent> {
public:
    DealResult dealResult;
    explicit DealCompletedEvent(const DealResult& dealResult) :
        dealResult {dealResult}
    {
    }
};
class DealPassedOutEvent : public sc::event<DealPassedOutEvent> {};

////////////////////////////////////////////////////////////////////////////////
// Shuffling
////////////////////////////////////////////////////////////////////////////////

class InDeal;

Shuffling::Shuffling(my_context ctx) :
    my_base {ctx}
{
    auto& context = outermost_context();
    if (dereference(context.gameManager).hasEnded()) {
        post_event(GameEndedEvent {});
    } else {
        dereference(context.cardManager).requestShuffle();
    }
}

sc::result Shuffling::react(const ShuffledEvent&)
{
    const auto& card_manager = dereference(outermost_context().cardManager);
    if (card_manager.getNumberOfCards() == N_CARDS) {
        return transit<InDeal>();
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// InDeal
////////////////////////////////////////////////////////////////////////////////

class InBidding;

class InDeal : public sc::state<InDeal, BridgeEngine, InBidding> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<DealCompletedEvent>,
        sc::custom_reaction<DealPassedOutEvent>>;

    InDeal(my_context ctx);

    ~InDeal();

    sc::result react(const DealCompletedEvent&);
    sc::result react(const DealPassedOutEvent&);

    Bidding& getBidding();
    const Bidding& getBidding() const;

    Hand& getHand(Position position);
    const Hand& getHand(Position position) const;
    auto getHands(Position first = Position::NORTH) const;

    Position getPosition(const Hand& player) const;

private:

    template<typename... Args1, typename... Args2>
    sc::result internalCallAndTransit(
        void (GameManager::*memfn)(Args1...),
        Args2&&... args);

    std::unique_ptr<Bidding> bidding;
    std::vector<std::shared_ptr<Hand>> hands;
    boost::bimaps::bimap<Position, Hand*> handsMap;
};

InDeal::InDeal(my_context ctx) :
    my_base {ctx}
{
    auto& context = outermost_context();
    auto& card_manager = dereference(context.cardManager);
    for (const auto position : POSITIONS) {
        const auto ns = cardsFor(position);
        hands.emplace_back(card_manager.getHand(ns.begin(), ns.end()));
        handsMap.insert({position, hands.back().get()});
    }
    assert(hands.size() == N_POSITIONS);
    const auto& game_manager = dereference(context.gameManager);
    const auto opener_position = dereference(game_manager.getOpenerPosition());
    bidding = std::make_unique<BasicBidding>(opener_position);
}

InDeal::~InDeal() = default;

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
    auto func = [&](const Position position) -> decltype(auto)
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
        getNumberOfTricksWon(event.dealResult, declarer_partnership));
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
    auto& game_manager = dereference(outermost_context().gameManager);
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

class Playing : public sc::simple_state<Playing, InDeal, PlayingTrick> {
public:
    std::size_t getNumberOfTricksPlayed() const;

    DealResult getDealResult() const;

private:

    void addTrick(std::unique_ptr<Trick>&& trick);
    Position getLeaderPosition() const;
    boost::optional<Suit> getTrump() const;

    std::vector<std::unique_ptr<Trick>> tricks;

    friend class PlayingTrick;
};

void Playing::addTrick(std::unique_ptr<Trick>&& trick)
{
    tricks.emplace_back(std::move(trick));
    if (tricks.size() == N_CARDS_PER_PLAYER) {
        post_event(DealCompletedEvent {getDealResult()});
    } else {
        post_event(NewTrickEvent {});
    }
}

Position Playing::getLeaderPosition() const
{
    if (tricks.empty()) {
        const auto declarer_position = dereference(
            dereference(context<InDeal>().getBidding().getDeclarerPosition()));
        return clockwise(declarer_position);
    }
    const auto& winner = dereference(tricks.back()->getWinner());
    return context<InDeal>().getPosition(winner);
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

std::size_t Playing::getNumberOfTricksPlayed() const
{
    return tricks.size();
}

DealResult Playing::getDealResult() const
{
    auto north_south = 0;
    auto east_west = 0;
    for (const auto& trick : tricks) {
        assert(trick);
        if (const auto& winner = trick->getWinner()) {
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

private:
    std::unique_ptr<Trick> trick;
};

PlayingTrick::PlayingTrick(my_context ctx) :
    my_base {ctx}
{
    const auto& hands = context<InDeal>().getHands(
        context<Playing>().getLeaderPosition());
    trick = std::make_unique<BasicTrick>(
        hands.begin(), hands.end(),
        context<Playing>().getTrump());
}

const Trick& PlayingTrick::getTrick() const
{
    assert(trick);
    return *trick;
}

sc::result PlayingTrick::react(const PlayCardEvent& event)
{
    assert(trick);
    const auto player_position = outermost_context().getPosition(event.player);
    auto& hand = context<InDeal>().getHand(player_position);
    const auto& card = hand.getCard(event.card);
    if (card && trick->play(hand, *card)) {
        hand.markPlayed(event.card);
        if (trick->isCompleted()) {
            context<Playing>().addTrick(std::move(trick));
        }
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// BridgeEngine
////////////////////////////////////////////////////////////////////////////////

void BridgeEngine::init()
{
    for (const auto t : zip(POSITIONS, players)) {
        playersMap.insert({t.get<0>(), t.get<1>().get()});
    }
    assert(playersMap.size() == N_POSITIONS);
}

BridgeEngine::~BridgeEngine() = default;

boost::optional<Vulnerability> BridgeEngine::getVulnerability() const
{
    return dereference(gameManager).getVulnerability();
}

boost::optional<const Player&> BridgeEngine::getPlayerInTurn() const
{
    if (state_cast<const InBidding*>()) {
        const auto& bidding = state_cast<const InDeal&>().getBidding();
        return getPlayer(dereference(bidding.getPositionInTurn()));
    } else if (const auto* state = state_cast<const PlayingTrick*>()) {
        const auto& hand = dereference(state->getTrick().getHandInTurn());
        return getPlayer(state_cast<const InDeal&>().getPosition(hand));
    }
    return boost::none;
}

const Player& BridgeEngine::getPlayer(const Position position) const
{
    const auto& player = playersMap.left.at(position);
    return dereference(player);
}

Position BridgeEngine::getPosition(const Player& player) const
{
    // This const_cast is safe as the player is only used as key
    return playersMap.right.at(const_cast<Player*>(&player));
}

boost::optional<const Hand&> BridgeEngine::getHand(const Player& player) const
{
    const auto position = getPosition(player);
    return internalCallIfInState(*this, &InDeal::getHand, position);
}

boost::optional<const Bidding&> BridgeEngine::getBidding() const
{
    return internalCallIfInState(*this, &InDeal::getBidding);
}

boost::optional<const Trick&> BridgeEngine::getCurrentTrick() const
{
    return internalCallIfInState(*this, &PlayingTrick::getTrick);
}

boost::optional<std::size_t> BridgeEngine::getNumberOfTricksPlayed() const
{
    return internalCallIfInState(*this, &Playing::getNumberOfTricksPlayed);
}

boost::optional<DealResult> BridgeEngine::getDealResult() const
{
    return internalCallIfInState(*this, &Playing::getDealResult);
}

}
}
