#include "engine/BridgeEngine.hh"

#include "bridge/BasicBidding.hh"
#include "bridge/BasicTrick.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Player.hh"
#include "bridge/TricksWon.hh"
#include "engine/CardManager.hh"
#include "FunctionObserver.hh"
#include "FunctionQueue.hh"
#include "Utility.hh"

#include <boost/range/adaptor/transformed.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/range/combine.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/in_state_reaction.hpp>
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

template<typename T>
std::optional<Position> getPositionHelper(
    const std::vector<std::shared_ptr<T>> ts, const T& t)
{
    const auto iter = std::find_if(
        ts.begin(), ts.end(), [&t](const auto& tp) { return &t == tp.get(); });
    if (iter != ts.end()) {
        return static_cast<PositionLabel>(iter - ts.begin());
    }
    return std::nullopt;
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
class DealEndedEvent : public sc::event<DealEndedEvent> {};
class NewPlayEvent : public sc::event<NewPlayEvent> {};
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
class CardRevealedEvent : public sc::event<CardRevealedEvent> {};
class DummyHandRevealedEvent : public sc::event<DummyHandRevealedEvent> {};
class DummyRevealedEvent : public sc::event<DummyRevealedEvent> {};
class ShufflingCompletedEvent : public sc::event<ShufflingCompletedEvent> {};

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

    template<typename EventGenerator>
    auto makeCardRevealStateObserver(EventGenerator&& generator);
    void requestShuffle();

    bool setPlayer(Position position, std::shared_ptr<Player> player);
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
    std::vector<
        std::pair<
            std::reference_wrapper<const Trick>,
            std::optional<Position>>> getTricks() const;
    const Hand* getDummyHandIfVisible() const;

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
    gameManager {std::move(gameManager)},
    players(Position::size())
{
}

template<typename EventGenerator>
auto BridgeEngine::Impl::makeCardRevealStateObserver(EventGenerator&& generator)
{
    // EventGenerator is a function that gets called when the observer is
    // notified with completed card reveal event. Its parameter is the list of
    // revealed cards. It's expected to return optional event that is
    // subsequently processed, or none if, after examining the revealed cards it
    // decides no event is processed.
    return makeObserver<Hand::CardRevealState, Hand::IndexVector>(
        [this, generator = std::forward<EventGenerator>(generator)](
            const auto& state, const auto& ns)
        {
            if (state == Hand::CardRevealState::COMPLETED) {
                if (auto event = generator(ns)) {
                    functionQueue(
                        [this, event = std::move(*event)]()
                        {
                            this->process_event(event);
                        });
                }
            }
        });
}

void BridgeEngine::Impl::requestShuffle()
{
    dereference(cardManager).requestShuffle();
}

// Find other method definitions later (they refer to states)

////////////////////////////////////////////////////////////////////////////////
// Idle
////////////////////////////////////////////////////////////////////////////////

class Shuffling;

class Idle : public sc::simple_state<Idle, BridgeEngine::Impl> {
public:
    using reactions = sc::custom_reaction<StartDealEvent>;
    sc::result react(const StartDealEvent&);
};

sc::result Idle::react(const StartDealEvent&)
{
    auto& context = outermost_context();
    if (context.getGameManager().hasEnded()) {
        return terminate();
    } else {
        context.requestShuffle();
        return transit<Shuffling>();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Shuffling
////////////////////////////////////////////////////////////////////////////////

class InDeal;

class Shuffling : public sc::simple_state<Shuffling, BridgeEngine::Impl> {
public:
    using reactions = sc::transition<ShufflingCompletedEvent, InDeal>;
};

////////////////////////////////////////////////////////////////////////////////
// InDeal
////////////////////////////////////////////////////////////////////////////////

class InBidding;

class InDeal : public sc::state<InDeal, BridgeEngine::Impl, InBidding> {
public:
    using reactions = sc::transition<DealEndedEvent, Idle>;

    InDeal(my_context ctx);
    void exit();

    Bidding& getBidding();
    const Bidding& getBidding() const;

    Hand& getHand(Position position);
    const Hand& getHand(Position position) const;
    auto getHands(Position first = Positions::NORTH) const;
    std::optional<Position> getPosition(const Hand& player) const;
    Hand& getHandConstCastSafe(const Hand& hand);

    void addTrick(Position leader);
    bool isLastTrick() const;
    Trick* getCurrentTrick();
    const Trick* getCurrentTrick() const;
    std::optional<std::optional<Suit>> getTrump() const;
    auto getTricks() const;
    TricksWon getTricksWon() const;

private:

    auto internalMakeBidding();
    auto internalMakeHands();

    const std::unique_ptr<Bidding> bidding;
    const std::vector<std::shared_ptr<Hand>> hands;
    std::vector<std::unique_ptr<Trick>> tricks;
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
        boost::make_transform_iterator(Position::begin(), func),
        boost::make_transform_iterator(Position::end(),   func));
}

InDeal::InDeal(my_context ctx) :
    my_base {ctx},
    bidding {internalMakeBidding()},
    hands {internalMakeHands()}
{
    auto& context = outermost_context();
    const auto& game_manager = context.getGameManager();
    const auto opener_position = dereference(game_manager.getOpenerPosition());
    context.getDealStartedNotifier().notifyAll(
        BridgeEngine::DealStarted {
            opener_position,
            dereference(game_manager.getVulnerability())});
}

void InDeal::exit()
{
    auto& context = outermost_context();
    const auto tricks_won = getTricksWon();
    auto deal_result = GameManager::ResultType {};

    assert(bidding);
    const auto declarer = bidding->getDeclarerPosition();
    const auto contract = bidding->getContract();
    if (!declarer || !*declarer || !contract || !*contract) {
        deal_result = context.getGameManager().addPassedOut();
    } else {
        const auto declarer_partnership = partnershipFor(**declarer);
        deal_result = context.getGameManager().addResult(
            declarer_partnership, **contract,
            getNumberOfTricksWon(tricks_won, declarer_partnership));
    }
    context.getDealEndedNotifier().notifyAll(
        BridgeEngine::DealEnded {deal_result});
}

Bidding& InDeal::getBidding()
{
    assert(bidding);
    return *bidding;
}

const Bidding& InDeal::getBidding() const
{
    // This const_cast is safe as we are not actually writing to any member
    // variable in non-const version
    return const_cast<InDeal&>(*this).getBidding();
}

Hand& InDeal::getHand(const Position position)
{
    const auto n = static_cast<std::size_t>(position.get());
    return dereference(hands.at(n));
}

const Hand& InDeal::getHand(const Position position) const
{
    // This const_cast is safe as we are not actually writing to any member
    // variable in non-const version
    return const_cast<InDeal&>(*this).getHand(position);
}

auto InDeal::getHands(const Position first) const
{
    auto positions = std::array<Position, Position::size()> {};
    std::copy(Position::begin(), Position::end(), positions.begin());
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

std::optional<Position> InDeal::getPosition(const Hand& hand) const
{
    return getPositionHelper(hands, hand);
}

Hand& InDeal::getHandConstCastSafe(const Hand& hand)
{
    // The purpose of this method if to strip the constness of the hand pointer,
    // but only if the hand is legitimately taking part of the deal
    return getHand(dereference(getPosition(hand)));
}

void InDeal::addTrick(const Position leader)
{
    const auto& hands = getHands(leader);
    tricks.emplace_back(
        std::make_unique<BasicTrick>(hands.begin(), hands.end()));
}

bool InDeal::isLastTrick() const
{
    return tricks.size() == N_CARDS_PER_PLAYER;
}

Trick* InDeal::getCurrentTrick()
{
    return tricks.empty() ? nullptr : tricks.back().get();
}

const Trick* InDeal::getCurrentTrick() const
{
    // This const_cast is safe as we are not actually writing to any member
    // variable in non-const version
    return const_cast<InDeal&>(*this).getCurrentTrick();
}

std::optional<std::optional<Suit>> InDeal::getTrump() const
{
    const auto contract = getBidding().getContract();
    if (!contract || !*contract) {
        return std::nullopt;
    }
    switch ((*contract)->bid.strain.get()) {
    case StrainLabel::CLUBS:
        return Suits::CLUBS;
    case StrainLabel::DIAMONDS:
        return Suits::DIAMONDS;
    case StrainLabel::HEARTS:
        return Suits::HEARTS;
    case StrainLabel::SPADES:
        return Suits::SPADES;
    default:
        return std::optional<Suit> {};
    }
}

auto InDeal::getTricks() const
{
    return boost::adaptors::transform(
        tricks,
        [this, trump = getTrump()](const auto& trick_ptr)
        {
            const auto winner = getWinner(*trick_ptr, dereference(trump));
            const auto winner_position = winner ?
                std::optional {getPosition(*winner)} : std::nullopt;
            return std::pair {std::cref(*trick_ptr), winner_position};
        });
}

TricksWon InDeal::getTricksWon() const
{
    auto north_south = 0;
    auto east_west = 0;
    for (const auto trick_winner_pair : getTricks()) {
        if (const auto winner_position = trick_winner_pair.second) {
            const auto winner_partnership = partnershipFor(*winner_position);
            switch (winner_partnership.get()) {
            case PartnershipLabel::NORTH_SOUTH:
                ++north_south;
                break;
            case PartnershipLabel::EAST_WEST:
                ++east_west;
            }
        }
    }
    return {north_south, east_west};
}

////////////////////////////////////////////////////////////////////////////////
// InBidding
////////////////////////////////////////////////////////////////////////////////

class Playing;

class InBidding : public sc::state<InBidding, InDeal> {
public:
    using reactions = sc::custom_reaction<CallEvent>;

    InBidding(my_context ctx);

    sc::result react(const CallEvent&);
};

InBidding::InBidding(my_context ctx) :
    my_base {ctx}
{
    outermost_context().getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {
            context<InDeal>().getBidding().getOpeningPosition()});
}

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
                post_event(DealEndedEvent {});
            }
        }
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// Playing
////////////////////////////////////////////////////////////////////////////////

class SelectingCard;
class DummyNotVisible;
class DummyVisible;

class Playing : public sc::state<
    Playing, InDeal, boost::mpl::list<SelectingCard, DummyNotVisible>> {
public:

    Playing(my_context ctx);

    Hand& getDummyHand();
    const Hand& getDummyHand() const;
    std::optional<Position> getPositionInTurn() const;
    const Hand* getHandInTurn() const;
    Hand* getHandIfHasTurn(const Player& player, const Hand& hand);

    void notifyTurnStarted();
    void commitPlay(const PlayCardEvent& event);
    void play(const CardRevealedEvent&);
    void requestRevealDummy(const NewPlayEvent&);
    void notifyDummyRevealed(const DummyHandRevealedEvent&);

private:

    Position getDeclarerPosition() const;
    Position getDummyPosition() const;

    std::optional<std::pair<Hand&, int>> commitedPlay;
    std::shared_ptr<Hand::CardRevealStateObserver> cardRevealStateObserver;
};

Playing::Playing(my_context ctx)  :
    my_base {ctx}
{
    context<InDeal>().addTrick(clockwise(getDeclarerPosition()));
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

std::optional<Position> Playing::getPositionInTurn() const
{
    if (const auto* hand = getHandInTurn()) {
        auto position = dereference(context<InDeal>().getPosition(*hand));
        // Declarer plays for dummy
        if (position == getDummyPosition()) {
            position = partnerFor(position);
        }
        return position;
    }
    return std::nullopt;
}

const Hand* Playing::getHandInTurn() const
{
    if (const auto* trick = context<InDeal>().getCurrentTrick()) {
        return trick->getHandInTurn();
    }
    return nullptr;
}

Hand* Playing::getHandIfHasTurn(const Player& player, const Hand& hand)
{
    if (outermost_context().getPosition(player) == getPositionInTurn() &&
        &hand == getHandInTurn()) {
        return &context<InDeal>().getHandConstCastSafe(hand);
    }
    return nullptr;
}

void Playing::notifyTurnStarted()
{
    outermost_context().getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {dereference(getPositionInTurn())});
}

void Playing::commitPlay(const PlayCardEvent& event)
{
    auto& hand = context<InDeal>().getHandConstCastSafe(event.hand);
    commitedPlay.emplace(hand, event.card);
    cardRevealStateObserver = outermost_context().makeCardRevealStateObserver(
        [expected_card = event.card](const auto& ns)
            -> std::optional<CardRevealedEvent>
        {
            if (ns.size() == 1 && ns.front() == expected_card) {
                return CardRevealedEvent {};
            }
            return std::nullopt;
        });
    hand.subscribe(cardRevealStateObserver);
    hand.requestReveal(&event.card, &event.card+1);
}

void Playing::play(const CardRevealedEvent&)
{
    auto& indeal = context<InDeal>();
    assert(commitedPlay);
    auto& trick = dereference(indeal.getCurrentTrick());
    auto& [hand, n_card] = *commitedPlay;
    const auto& card = dereference(hand.getCard(n_card));
    if (trick.play(hand, card)) {
        auto& context_ = outermost_context();
        hand.markPlayed(n_card);
        context_.getCardPlayedNotifier().notifyAll(
            BridgeEngine::CardPlayed { hand, card });
        if (trick.isCompleted()) {
            const auto& winner_hand = dereference(
                getWinner(trick, dereference(indeal.getTrump())));
            outermost_context().getTrickCompletedNotifier().notifyAll(
                BridgeEngine::TrickCompleted {trick, winner_hand});
            if (indeal.isLastTrick()) {
                post_event(DealEndedEvent {});
                return;
            } else {
                indeal.addTrick(dereference(indeal.getPosition(winner_hand)));
            }
        }
        post_event(NewPlayEvent {});
    }
}

void Playing::requestRevealDummy(const NewPlayEvent&)
{
    cardRevealStateObserver = outermost_context().makeCardRevealStateObserver(
        [](const auto& ns) -> std::optional<DummyHandRevealedEvent>
        {
            const auto expected_cards = to(N_CARDS_PER_PLAYER);
            const auto expected_cards_revealed = std::is_permutation(
                ns.begin(), ns.end(),
                expected_cards.begin(), expected_cards.end());
            if (expected_cards_revealed) {
                return DummyHandRevealedEvent {};
            }
            return std::nullopt;
        });
    auto& dummy_hand = getDummyHand();
    dummy_hand.subscribe(cardRevealStateObserver);
    requestRevealHand(dummy_hand);
}

void Playing::notifyDummyRevealed(const DummyHandRevealedEvent&)
{
    post_event(DummyRevealedEvent {});
    outermost_context().getDummyRevealedNotifier().notifyAll(
        BridgeEngine::DummyRevealed {getDummyPosition(), getDummyHand()});
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

////////////////////////////////////////////////////////////////////////////////
// SelectingCard
////////////////////////////////////////////////////////////////////////////////

class WaitingRevealCard;

class SelectingCard : public sc::state<SelectingCard, Playing> {
public:
    using reactions = sc::custom_reaction<PlayCardEvent>;

    SelectingCard(my_context ctx);

    sc::result react(const PlayCardEvent&);
};

SelectingCard::SelectingCard(my_context ctx) :
    my_base {ctx}
{
    context<Playing>().notifyTurnStarted();
}

sc::result SelectingCard::react(const PlayCardEvent& event)
{
    auto& playing = context<Playing>();
    const auto hand = playing.getHandIfHasTurn(event.player, event.hand);
    if (hand && canBePlayedFromHand(*hand, event.card)) {
        event.ret = true;
        return transit<WaitingRevealCard>(&Playing::commitPlay, event);
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// WaitingRevealCard
////////////////////////////////////////////////////////////////////////////////

class WaitingRevealDummy;

class WaitingRevealCard : public sc::simple_state<WaitingRevealCard, Playing> {
public:
    using reactions = boost::mpl::list<
      sc::in_state_reaction<CardRevealedEvent, Playing, &Playing::play>,
      sc::custom_reaction<NewPlayEvent>>;
    sc::result react(const CardRevealedEvent&);
    sc::result react(const NewPlayEvent&);
};

sc::result WaitingRevealCard::react(const NewPlayEvent& event)
{
    if (state_cast<const DummyVisible*>()) {
        return transit<SelectingCard>();
    } else {
        return transit<WaitingRevealDummy>(&Playing::requestRevealDummy, event);
    }
}

////////////////////////////////////////////////////////////////////////////////
// WaitingRevealDummy
////////////////////////////////////////////////////////////////////////////////

class WaitingRevealDummy :
    public sc::simple_state<WaitingRevealDummy, Playing> {
public:
    using reactions = sc::transition<DummyRevealedEvent, SelectingCard>;
};

////////////////////////////////////////////////////////////////////////////////
// DummyNotVisible
////////////////////////////////////////////////////////////////////////////////

class DummyNotVisible : public sc::simple_state<
    DummyNotVisible, Playing::orthogonal<1>> {
public:
    using reactions = sc::transition<
        DummyHandRevealedEvent, DummyVisible,
        Playing, &Playing::notifyDummyRevealed>;
};

////////////////////////////////////////////////////////////////////////////////
// DummyVisible
////////////////////////////////////////////////////////////////////////////////

class DummyVisible : public sc::simple_state<
    DummyVisible, Playing::orthogonal<1>> {
public:
    const Hand& getDummyHand() const;
};

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

bool BridgeEngine::Impl::setPlayer(
    const Position position, std::shared_ptr<Player> player)
{
    // Ensure player is not already in the game (unless trying the
    // no-op of adding the player back to its own seat)
    if (player) {
        for (const auto p : Position::all()) {
            if (p != position && getPlayer(p) == player.get()) {
                return false;
            }
        }
    }
    const auto n = static_cast<std::size_t>(position.get());
    players[n] = std::move(player);
    return true;
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
    } else if (const auto* state = state_cast<const Playing*>()) {
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
    if (const auto* state = state_cast<const Playing*>()) {
        return state->getHandInTurn();
    }
    return nullptr;
}

const Player* BridgeEngine::Impl::getPlayer(const Position position) const
{
    const auto n = static_cast<std::size_t>(position.get());
    return (n < Position::size()) ? players[n].get() : nullptr;
}

std::optional<Position> BridgeEngine::Impl::getPosition(
    const Player& player) const
{
    return getPositionHelper(players, player);
}

const Hand* BridgeEngine::Impl::getHand(const Position position) const
{
    return internalCallIfInState(&InDeal::getHand, position);
}

std::optional<Position> BridgeEngine::Impl::getPosition(
    const Hand& hand) const
{
    if (const auto* state = state_cast<const InDeal*>()) {
        return state->getPosition(hand);
    }
    return std::nullopt;
}

const Bidding* BridgeEngine::Impl::getBidding() const
{
    return internalCallIfInState(&InDeal::getBidding);
}

const Trick* BridgeEngine::Impl::getCurrentTrick() const
{
    if (const auto* state = state_cast<const InDeal*>()) {
        return state->getCurrentTrick();
    }
    return nullptr;
}

std::vector<
    std::pair<
        std::reference_wrapper<const Trick>,
        std::optional<Position>>> BridgeEngine::Impl::getTricks() const
{
    if (const auto* state = state_cast<const InDeal*>()) {
        const auto tricks = state->getTricks();
        return std::vector(tricks.begin(), tricks.end());
    }
    return {};
}

const Hand* BridgeEngine::Impl::getDummyHandIfVisible() const
{
    return internalCallIfInState(&DummyVisible::getDummyHand);
}

void BridgeEngine::Impl::handleNotify(const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::COMPLETED &&
        getCardManager().getNumberOfCards() == N_CARDS) {
        functionQueue(
            [this]()
            {
                process_event(ShufflingCompletedEvent {});
            });
    }
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

bool BridgeEngine::setPlayer(
    const Position position, std::shared_ptr<Player> player)
{
    assert(impl);
    return impl->setPlayer(position, std::move(player));
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

bool BridgeEngine::isVisibleToAll(const Hand& hand) const
{
    assert(impl);
    return &hand == impl->getDummyHandIfVisible();
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

std::vector<
    std::pair<
        std::reference_wrapper<const Trick>,
        std::optional<Position>>> BridgeEngine::getTricks() const
{
    assert(impl);
    return impl->getTricks();
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

BridgeEngine::DummyRevealed::DummyRevealed(
    const Position position, const Hand& hand) :
    position {position},
    hand {hand}
{
}

BridgeEngine::DealEnded::DealEnded(const GameManager::ResultType& result) :
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
    const BridgeEngine::DummyRevealed& lhs,
    const BridgeEngine::DummyRevealed& rhs)
{
    return lhs.position == rhs.position && &lhs.hand == &rhs.hand;
}

}
}
