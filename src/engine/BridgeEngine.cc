#include "engine/BridgeEngine.hh"

#include "bridge/BasicBidding.hh"
#include "bridge/BasicTrick.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/Deal.hh"
#include "bridge/Hand.hh"
#include "bridge/Partnership.hh"
#include "bridge/Player.hh"
#include "bridge/TricksWon.hh"
#include "bridge/UuidGenerator.hh"
#include "engine/CardManager.hh"
#include "FunctionObserver.hh"
#include "FunctionQueue.hh"
#include "Utility.hh"

#include <boost/range/adaptor/transformed.hpp>
#include <boost/iterator/transform_iterator.hpp>
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

UuidGenerator uuidGenerator;

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
    const GameManager& getGameManager() const { return dereference(gameManager); }
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

    const Deal* getCurrentDeal() const;
    const Player* getPlayerInTurn() const;
    const Hand* getHandInTurn() const;
    const Player* getPlayer(Position position) const;
    std::optional<Position> getPosition(const Player& player) const;
    const Hand* getHand(Position position) const;
    const Bidding* getBidding() const;
    const Trick* getCurrentTrick() const;
    std::vector<
        std::pair<
            std::reference_wrapper<const Trick>,
            std::optional<Position>>> getTricks() const;

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
class Playing;

class InDeal :
    public sc::state<InDeal, BridgeEngine::Impl, InBidding>,
    public Deal {
public:
    using reactions = sc::transition<DealEndedEvent, Idle>;

    InDeal(my_context ctx);
    void exit();

    void notifyTurnStarted();

    Bidding& getBiddingNonConst();

    Hand& getHandNonConst(Position position);
    auto getHands(Position first = Positions::NORTH) const;
    std::optional<Position> getPosition(const Hand& player) const;
    Hand& getHandConstCastSafe(const Hand& hand);

    void addTrick(Position leader);
    bool isLastTrick() const;
    Trick* getCurrentTrickNonConst();
    std::optional<std::optional<Suit>> getTrump() const;
    auto getTricks() const;
    TricksWon getTricksWon() const;

private:

    auto internalMakeBidding();
    auto internalMakeHands();

    const Uuid& handleGetUuid() const override;
    DealPhase handleGetPhase() const override;
    Vulnerability handleGetVulnerability() const override;
    const Hand& handleGetHand(Position position) const override;
    const Bidding& handleGetBidding() const override;
    int handleGetNumberOfTricks() const override;
    TrickPositionPair handleGetTrick(int n) const override;

    const Uuid uuid;
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
    uuid {uuidGenerator()},
    bidding {internalMakeBidding()},
    hands {internalMakeHands()}
{
    auto& context = outermost_context();
    const auto& game_manager = context.getGameManager();
    const auto opener_position = dereference(game_manager.getOpenerPosition());
    context.getDealStartedNotifier().notifyAll(
        BridgeEngine::DealStarted {
            uuid,
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
        BridgeEngine::DealEnded {uuid, deal_result});
}

void InDeal::notifyTurnStarted()
{
    outermost_context().getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {uuid, dereference(getPositionInTurn())});
}

Bidding& InDeal::getBiddingNonConst()
{
    assert(bidding);
    return *bidding;
}

Hand& InDeal::getHandNonConst(const Position position)
{
    return const_cast<Hand&>(getHand(position));
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
    return getHandNonConst(dereference(getPosition(hand)));
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

Trick* InDeal::getCurrentTrickNonConst()
{
    return tricks.empty() ? nullptr : tricks.back().get();
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

const Uuid& InDeal::handleGetUuid() const
{
    return uuid;
}

DealPhase InDeal::handleGetPhase() const
{
    if (state_cast<const InBidding*>()) {
        return DealPhase::BIDDING;
    }
    return DealPhase::PLAYING;
}

Vulnerability InDeal::handleGetVulnerability() const
{
    const auto& game_manager = outermost_context().getGameManager();
    return dereference(game_manager.getVulnerability());
}

const Hand& InDeal::handleGetHand(const Position position) const
{
    const auto n = static_cast<std::size_t>(position.get());
    return dereference(hands.at(n));
}

const Bidding& InDeal::handleGetBidding() const
{
    assert(bidding);
    return *bidding;
}

int InDeal::handleGetNumberOfTricks() const
{
    return static_cast<int>(tricks.size());
}

Deal::TrickPositionPair InDeal::handleGetTrick(const int n) const
{
    const auto trump = dereference(getTrump());
    assert(0 <= n && n < ssize(tricks));
    assert(tricks[n]);
    const auto& trick = *tricks[n];
    const auto winner = getWinner(trick, trump);
    const auto winner_position = winner ?
        std::optional {getPosition(*winner)} : std::nullopt;
    return {std::cref(trick), winner_position};
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
    const auto& in_deal = context<InDeal>();
    outermost_context().getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {
            in_deal.getUuid(), in_deal.getBidding().getOpeningPosition()});
}

sc::result InBidding::react(const CallEvent& event)
{
    auto& in_deal = context<InDeal>();
    auto& bidding = in_deal.getBiddingNonConst();
    const auto position = outermost_context().getPosition(event.player);
    if (position && bidding.call(*position, event.call)) {
        event.ret = true;
        auto& context = outermost_context();
        const auto& deal_uuid = in_deal.getUuid();
        context.getCallMadeNotifier().notifyAll(
            BridgeEngine::CallMade {
                deal_uuid, *position, event.call });
        if (const auto next_position_in_turn = bidding.getPositionInTurn()) {
            context.getTurnStartedNotifier().notifyAll(
                BridgeEngine::TurnStarted(deal_uuid, *next_position_in_turn));
        }
        const auto outer_contract = bidding.getContract();
        const auto outer_declarer_position = bidding.getDeclarerPosition();
        if (outer_contract && outer_declarer_position) {
            const auto inner_contract = *outer_contract;
            const auto inner_declarer_position = *outer_declarer_position;
            if (inner_contract && inner_declarer_position) {
                context.getBiddingCompletedNotifier().notifyAll(
                    BridgeEngine::BiddingCompleted {
                        deal_uuid, *inner_declarer_position, *inner_contract});
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
    return context<InDeal>().getHandNonConst(getDummyPosition());
}

const Hand& Playing::getDummyHand() const
{
    // This const_cast is safe as we are not actually writing to any member
    // variable in non-const version
    return const_cast<Playing&>(*this).getDummyHand();
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
    auto& trick = dereference(indeal.getCurrentTrickNonConst());
    auto& [hand, n_card] = *commitedPlay;
    const auto& card = dereference(hand.getCard(n_card));
    if (trick.play(hand, card)) {
        auto& context_ = outermost_context();
        const auto& deal_uuid = indeal.getUuid();
        hand.markPlayed(n_card);
        context_.getCardPlayedNotifier().notifyAll(
            BridgeEngine::CardPlayed {
                deal_uuid, dereference(indeal.getPosition(hand)), card });
        if (trick.isCompleted()) {
            const auto& winner_hand = dereference(
                getWinner(trick, dereference(indeal.getTrump())));
            outermost_context().getTrickCompletedNotifier().notifyAll(
                BridgeEngine::TrickCompleted {
                    deal_uuid, trick,
                    dereference(indeal.getPosition(winner_hand))});
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
        BridgeEngine::DummyRevealed {
            context<InDeal>().getUuid(), getDummyPosition(), getDummyHand()});
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
    context<InDeal>().notifyTurnStarted();
}

sc::result SelectingCard::react(const PlayCardEvent& event)
{
    const auto& in_deal = context<InDeal>();
    const auto player_position = outermost_context().getPosition(event.player);
    const auto position_in_turn = in_deal.getPositionInTurn();
    if (player_position == position_in_turn) {
        const auto hand_in_turn = in_deal.getHandInTurn();
        if (&event.hand == hand_in_turn && canBePlayedFromHand(event.hand, event.card)) {
            event.ret = true;
            return transit<WaitingRevealCard>(&Playing::commitPlay, event);
        }
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

const Deal* BridgeEngine::Impl::getCurrentDeal() const
{
    return state_cast<const InDeal*>();
}

const Player* BridgeEngine::Impl::getPlayerInTurn() const
{
    if (const auto* deal = getCurrentDeal()) {
        if (const auto position = deal->getPositionInTurn()) {
            return getPlayer(*position);
        }
    }
    return nullptr;
}

const Hand* BridgeEngine::Impl::getHandInTurn() const
{
    if (const auto* state = state_cast<const InDeal*>()) {
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

const Deal* BridgeEngine::getCurrentDeal() const
{
    assert(impl);
    return impl->getCurrentDeal();
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

const Player* BridgeEngine::getPlayer(const Position position) const
{
    assert(impl);
    return impl->getPlayer(position);
}

std::optional<Position> BridgeEngine::getPosition(const Player& player) const
{
    assert(impl);
    return impl->getPosition(player);
}

BridgeEngine::DealStarted::DealStarted(
    const Uuid& uuid, const Position opener, const Vulnerability vulnerability) :
    uuid {uuid},
    opener {opener},
    vulnerability {vulnerability}
{
}

BridgeEngine::TurnStarted::TurnStarted(const Uuid& uuid, const Position position) :
    uuid {uuid},
    position {position}
{
}

BridgeEngine::CallMade::CallMade(const Uuid& uuid, const Position position, const Call& call) :
    uuid {uuid},
    position {position},
    call {call}
{
}

BridgeEngine::BiddingCompleted::BiddingCompleted(
    const Uuid& uuid, const Position declarer, const Contract& contract) :
    uuid {uuid},
    declarer {declarer},
    contract {contract}
{
}

BridgeEngine::CardPlayed::CardPlayed(
    const Uuid& uuid, const Position position, const Card& card) :
    uuid {uuid},
    position {position},
    card {card}
{
}

BridgeEngine::TrickCompleted::TrickCompleted(
    const Uuid& uuid, const Trick& trick, const Position winner) :
    uuid {uuid},
    trick {trick},
    winner {winner}
{
}

BridgeEngine::DummyRevealed::DummyRevealed(
    const Uuid& uuid, const Position position, const Hand& hand) :
    uuid {uuid},
    position {position},
    hand {hand}
{
}

BridgeEngine::DealEnded::DealEnded(
    const Uuid& uuid, const GameManager::ResultType& result) :
    uuid {uuid},
    result {result}
{
}

bool operator==(
    const BridgeEngine::DealStarted& lhs, const BridgeEngine::DealStarted& rhs)
{
    return lhs.uuid == rhs.uuid && lhs.opener == rhs.opener &&
        lhs.vulnerability == rhs.vulnerability;
}

bool operator==(
    const BridgeEngine::TurnStarted& lhs, const BridgeEngine::TurnStarted& rhs)
{
    return lhs.uuid == rhs.uuid && lhs.position == rhs.position;
}

bool operator==(
    const BridgeEngine::CallMade& lhs, const BridgeEngine::CallMade& rhs)
{
    return lhs.uuid == rhs.uuid &&
        lhs.position == rhs.position && lhs.call == rhs.call;
}
bool operator==(
    const BridgeEngine::BiddingCompleted& lhs,
    const BridgeEngine::BiddingCompleted& rhs)
{
    return lhs.uuid == rhs.uuid && lhs.declarer == rhs.declarer &&
        lhs.contract == rhs.contract;
}

bool operator==(
    const BridgeEngine::CardPlayed& lhs, const BridgeEngine::CardPlayed& rhs)
{
    return lhs.uuid == rhs.uuid && lhs.position == rhs.position &&
        &lhs.card == &rhs.card;
}

bool operator==(
    const BridgeEngine::TrickCompleted& lhs,
    const BridgeEngine::TrickCompleted& rhs)
{
    return lhs.uuid == rhs.uuid && &lhs.trick == &rhs.trick &&
        lhs.winner == rhs.winner;
}

bool operator==(
    const BridgeEngine::DummyRevealed& lhs,
    const BridgeEngine::DummyRevealed& rhs)
{
    return lhs.uuid == rhs.uuid && lhs.position == rhs.position &&
        &lhs.hand == &rhs.hand;
}

}
}
