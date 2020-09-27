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

#include <boost/iterator/transform_iterator.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/simple_state.hpp>
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

class StartDealEvent : public sc::event<StartDealEvent> {
public:
    StartDealEvent(const Deal* deal) :
        deal {deal}
    {
    }

    const Deal* deal;
};
class CreateNewDealEvent : public sc::event<CreateNewDealEvent> {};
class RecallDealEvent : public sc::event<RecallDealEvent> {
public:
    RecallDealEvent(const Deal& deal) :
        deal {deal}
        {
        }

    const Deal& deal;
};
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
class StartPlayingEvent : public sc::event<StartPlayingEvent> {
public:
    StartPlayingEvent(const Position declarer) :
        declarer {declarer}
    {
    }

    Position declarer;
};
class RecallPlayingEvent : public sc::event<RecallPlayingEvent> {
public:
    RecallPlayingEvent(const Deal& deal) :
        deal {deal}
    {
    }

    const Deal& deal;
};
class DealEndedEvent : public sc::event<DealEndedEvent> {
public:
    DealEndedEvent(
        const Uuid& dealUuid, const Contract& contract,
        const Partnership declarerPartnership, const int tricksWon) :
        dealUuid {dealUuid},
        contract {contract},
        declarerPartnership {declarerPartnership},
        tricksWon {tricksWon}
    {
    }

    Uuid dealUuid;
    Contract contract;
    Partnership declarerPartnership;
    int tricksWon;
};
class DealPassedOutEvent : public sc::event<DealPassedOutEvent> {
public:
    DealPassedOutEvent(const Uuid& dealUuid) :
        dealUuid {dealUuid}
    {
    }

    Uuid dealUuid;
};
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
    void endDeal(const DealEndedEvent& event);
    void passOutDeal(const DealPassedOutEvent& event);

    bool setPlayer(Position position, std::shared_ptr<Player> player);
    CardManager& getCardManager() { return dereference(cardManager); }
    const CardManager& getCardManager() const { return dereference(cardManager); }
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
    Observable<TrickStarted>& getTrickStartedNotifier()
    {
        return trickStartedNotifier;
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
    Observable<TrickStarted> trickStartedNotifier;
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

void BridgeEngine::Impl::endDeal(const DealEndedEvent& event)
{
    const auto deal_result = getGameManager().addResult(
        event.declarerPartnership, event.contract, event.tricksWon);
    getDealEndedNotifier().notifyAll(
        BridgeEngine::DealEnded {event.dealUuid, deal_result});
}

void BridgeEngine::Impl::passOutDeal(const DealPassedOutEvent& event)
{
    const auto deal_result = getGameManager().addPassedOut();
    getDealEndedNotifier().notifyAll(
        BridgeEngine::DealEnded {event.dealUuid, deal_result});
}

// Find other method definitions later (they refer to states)

////////////////////////////////////////////////////////////////////////////////
// Idle
////////////////////////////////////////////////////////////////////////////////

class Shuffling;
class InDeal;

class Idle : public sc::simple_state<Idle, BridgeEngine::Impl> {
public:
    using reactions = sc::custom_reaction<StartDealEvent>;
    sc::result react(const StartDealEvent& event);
};

sc::result Idle::react(const StartDealEvent& event)
{
    auto& context = outermost_context();
    if (context.getGameManager().hasEnded()) {
        return terminate();
    } else if (!event.deal) {
        context.requestShuffle();
        return transit<Shuffling>();
    } else {
        post_event(RecallDealEvent {*event.deal});
        return transit<InDeal>();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Shuffling
////////////////////////////////////////////////////////////////////////////////

class InDeal;

class Shuffling : public sc::simple_state<Shuffling, BridgeEngine::Impl> {
public:
    using reactions = sc::custom_reaction<ShufflingCompletedEvent>;
    sc::result react(const ShufflingCompletedEvent&);
};

sc::result Shuffling::react(const ShufflingCompletedEvent&)
{
    post_event(CreateNewDealEvent {});
    return transit<InDeal>();
}

////////////////////////////////////////////////////////////////////////////////
// InDeal
////////////////////////////////////////////////////////////////////////////////

class InBidding;
class InitializingDeal;

class InDeal :
    public sc::simple_state<InDeal, BridgeEngine::Impl, InitializingDeal>,
    public Deal {
public:
    void createNewDeal(const CreateNewDealEvent&);
    void recallDeal(const RecallDealEvent& event);
    void startPlaying(const StartPlayingEvent& event);
    void recallPlaying(const RecallPlayingEvent&);
    void notifyTurnStarted();

    Bidding& getBiddingNonConst();

    Hand& getHandNonConst(Position position);
    std::optional<Position> getPosition(const Hand& player) const;
    Hand& getHandConstCastSafe(const Hand& hand);

    void addTrick(Position leader);
    bool isLastTrick() const;
    Trick* getCurrentTrickNonConst();

private:

    auto internalCreateHands(Position first) const;
    auto internalCreateTrick(Position leader);
    auto internalMakeBidding();
    auto internalRecallBidding(const Bidding& bidding);
    auto internalMakeHands();
    auto internalRecallTrick(const Trick& trick, const Deal& deal);

    const Uuid& handleGetUuid() const override;
    DealPhase handleGetPhase() const override;
    Vulnerability handleGetVulnerability() const override;
    const Hand& handleGetHand(Position position) const override;
    const Card& handleGetCard(int n) const override;
    const Bidding& handleGetBidding() const override;
    int handleGetNumberOfTricks() const override;
    const Trick& handleGetTrick(int n) const override;

    Uuid uuid;
    std::unique_ptr<Bidding> bidding;
    std::vector<std::shared_ptr<Hand>> hands;
    std::vector<std::unique_ptr<Trick>> tricks;
};

auto InDeal::internalCreateHands(const Position first) const
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

auto InDeal::internalCreateTrick(const Position leader)
{
    const auto& hands = internalCreateHands(leader);
    return std::make_unique<BasicTrick>(hands.begin(), hands.end());
}

auto InDeal::internalMakeBidding()
{
    const auto& game_manager = outermost_context().getGameManager();
    return std::make_unique<BasicBidding>(
        dereference(game_manager.getOpenerPosition()));
}

auto InDeal::internalRecallBidding(const Bidding& bidding)
{
    auto ret = std::make_unique<BasicBidding>(bidding.getOpeningPosition());
    for (const auto& [position, call] : bidding) {
        if (!ret->call(position, call)) {
            throw BridgeEngineFailure {"Invalid call when recalling a deal"};
        }
    }
    return ret;
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

auto InDeal::internalRecallTrick(const Trick& trick, const Deal& deal)
{
    // Here we must be careful whether referring to hands and cards in
    // the recalled deal or in the bridge engine
    const auto dealer_position = deal.getPosition(trick.getLeader());
    if (!dealer_position) {
        throw BridgeEngineFailure {"Invalid trick when recalling a deal"};
    }
    auto ret = internalCreateTrick(*dealer_position);
    for (const auto& [hand_in_deal, card_in_deal] : trick) {
        if (const auto hand_position = deal.getPosition(hand_in_deal)) {
            auto& hand_in_engine = getHandNonConst(*hand_position);
            if (const auto card_type = card_in_deal.getType()) {
                if (const auto n = findFromHand(hand_in_engine, *card_type)) {
                    if (const auto card_in_engine = hand_in_engine.getCard(*n)) {
                        if (ret->play(hand_in_engine, *card_in_engine)) {
                            hand_in_engine.markPlayed(*n);
                            continue;
                        }
                    }
                }
            }
        }
        throw BridgeEngineFailure {"Invalid trick when recalling a deal"};
    }
    return ret;
}

void InDeal::createNewDeal(const CreateNewDealEvent&)
{
    uuid = uuidGenerator();
    bidding = internalMakeBidding();
    hands = internalMakeHands();
    auto& context = outermost_context();
    const auto& game_manager = context.getGameManager();
    const auto opener_position = dereference(game_manager.getOpenerPosition());
    context.getDealStartedNotifier().notifyAll(
        BridgeEngine::DealStarted {
            uuid,
            opener_position,
            dereference(game_manager.getVulnerability())});
    context.getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {uuid, bidding->getOpeningPosition()});
}

void InDeal::recallDeal(const RecallDealEvent& event)
{
    uuid = event.deal.getUuid();
    hands = internalMakeHands();
    bidding = internalRecallBidding(event.deal.getBidding());
    if (bidding->hasEnded()) {
        post_event(RecallPlayingEvent {event.deal});
    }
}

void InDeal::startPlaying(const StartPlayingEvent& event)
{
    const auto leader_position = clockwise(event.declarer);
    addTrick(leader_position);
    outermost_context().getTurnStartedNotifier().notifyAll(
        BridgeEngine::TurnStarted {uuid, leader_position});
}

void InDeal::recallPlaying(const RecallPlayingEvent& event)
{
    for (const auto n : to(event.deal.getNumberOfTricks())) {
        tricks.emplace_back(
            internalRecallTrick(event.deal.getTrick(n), event.deal));
    }
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
    tricks.emplace_back(internalCreateTrick(leader));
    outermost_context().getTrickStartedNotifier().notifyAll(
        BridgeEngine::TrickStarted {uuid, leader});
}

bool InDeal::isLastTrick() const
{
    return tricks.size() == N_CARDS_PER_PLAYER;
}

Trick* InDeal::getCurrentTrickNonConst()
{
    return tricks.empty() ? nullptr : tricks.back().get();
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

const Card& InDeal::handleGetCard(const int n) const
{
    const auto& card_manager = outermost_context().getCardManager();
    return dereference(card_manager.getCard(n));
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

const Trick& InDeal::handleGetTrick(const int n) const
{
    return dereference(tricks.at(n));
}

////////////////////////////////////////////////////////////////////////////////
// InitializingDeal
////////////////////////////////////////////////////////////////////////////////

class InitializingDeal : public sc::simple_state<InitializingDeal, InDeal> {
public:
    using reactions = boost::mpl::list<
        sc::transition<
            CreateNewDealEvent, InBidding, InDeal, &InDeal::createNewDeal>,
        sc::transition<
            RecallDealEvent, InBidding, InDeal, &InDeal::recallDeal>>;
};

////////////////////////////////////////////////////////////////////////////////
// InBidding
////////////////////////////////////////////////////////////////////////////////

class Playing;

class InBidding : public sc::simple_state<InBidding, InDeal> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<CallEvent>,
        sc::transition<
            StartPlayingEvent, Playing, InDeal, &InDeal::startPlaying>,
        sc::transition<
            RecallPlayingEvent, Playing, InDeal, &InDeal::recallPlaying>,
        sc::transition<
            DealPassedOutEvent, Idle, BridgeEngine::Impl,
            &BridgeEngine::Impl::passOutDeal>>;
    sc::result react(const CallEvent& event);
};

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
        const auto outer_contract = bidding.getContract();
        const auto outer_declarer_position = bidding.getDeclarerPosition();
        if (outer_contract && outer_declarer_position) {
            const auto inner_contract = *outer_contract;
            const auto inner_declarer_position = *outer_declarer_position;
            if (inner_contract && inner_declarer_position) {
                context.getBiddingCompletedNotifier().notifyAll(
                    BridgeEngine::BiddingCompleted {
                        deal_uuid, *inner_declarer_position, *inner_contract});
                post_event(StartPlayingEvent {*inner_declarer_position});
            } else {
                post_event(DealPassedOutEvent {deal_uuid});
            }
        } else {
            in_deal.notifyTurnStarted();
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

class Playing : public sc::simple_state<
    Playing, InDeal, boost::mpl::list<SelectingCard, DummyNotVisible>> {
public:
    using reactions = sc::transition<
        DealEndedEvent, Idle, BridgeEngine::Impl, &BridgeEngine::Impl::endDeal>;

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
    auto& in_deal = context<InDeal>();
    assert(commitedPlay);
    auto& trick = dereference(in_deal.getCurrentTrickNonConst());
    auto& [hand, n_card] = *commitedPlay;
    const auto& card = dereference(hand.getCard(n_card));
    if (trick.play(hand, card)) {
        auto& context_ = outermost_context();
        const auto& deal_uuid = in_deal.getUuid();
        hand.markPlayed(n_card);
        context_.getCardPlayedNotifier().notifyAll(
            BridgeEngine::CardPlayed {
                deal_uuid, dereference(in_deal.getPosition(hand)), card });
        if (trick.isCompleted()) {
            const auto n_tricks = in_deal.getNumberOfTricks();
            assert(n_tricks > 0);
            const auto winner_position = dereference(
                in_deal.getWinnerOfTrick(n_tricks - 1));
            outermost_context().getTrickCompletedNotifier().notifyAll(
                BridgeEngine::TrickCompleted {
                    deal_uuid, trick, winner_position});
            if (in_deal.isLastTrick()) {
                const auto& bidding = in_deal.getBidding();
                const auto declarer = dereference(
                    dereference(bidding.getDeclarerPosition()));
                const auto declarer_partnership = partnershipFor(declarer);
                const auto contract = dereference(
                    dereference(bidding.getContract()));
                const auto tricks_won = getNumberOfTricksWon(
                    in_deal.getTricksWon(), declarer_partnership);
                post_event(
                    DealEndedEvent {
                        deal_uuid, contract, declarer_partnership, tricks_won});
                return;
            } else {
                in_deal.addTrick(winner_position);
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
    auto& in_deal = context<InDeal>();
    outermost_context().getDummyRevealedNotifier().notifyAll(
        BridgeEngine::DummyRevealed {
            in_deal.getUuid(), getDummyPosition(), getDummyHand()});
    in_deal.notifyTurnStarted();
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

class SelectingCard : public sc::simple_state<SelectingCard, Playing> {
public:
    using reactions = sc::custom_reaction<PlayCardEvent>;
    sc::result react(const PlayCardEvent&);
};

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
        context<InDeal>().notifyTurnStarted();
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

void BridgeEngine::subscribeToTrickStarted(
    std::weak_ptr<Observer<TrickStarted>> observer)
{
    assert(impl);
    impl->getTrickStartedNotifier().subscribe(std::move(observer));
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

void BridgeEngine::startDeal(const Deal* deal)
{
    assert(impl);
    impl->functionQueue(
        [&impl = *impl, deal]()
        {
            impl.process_event(StartDealEvent {deal});
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

BridgeEngine::TrickStarted::TrickStarted(
    const Uuid& uuid, const Position leader) :
    uuid {uuid},
    leader {leader}
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
    const BridgeEngine::TrickStarted& lhs,
    const BridgeEngine::TrickStarted& rhs)
{
    return lhs.uuid == rhs.uuid && lhs.leader == rhs.leader;
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
