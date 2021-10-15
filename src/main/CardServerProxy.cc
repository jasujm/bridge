#include "main/CardServerProxy.hh"

#include "bridge/BasicHand.hh"
#include "bridge/CardsForPosition.hh"
#include "bridge/Position.hh"
#include "bridge/RevealableCard.hh"
#include "cardserver/Commands.hh"
#include "cardserver/PeerEntry.hh"
#include "engine/CardManager.hh"
#include "main/Commands.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
#include "messaging/Replies.hh"
#include "messaging/Security.hh"
#include "messaging/SerializationUtility.hh"
#include "Enumerate.hh"
#include "FunctionObserver.hh"
#include "FunctionQueue.hh"
#include "HexUtility.hh"
#include "Logging.hh"
#include "Utility.hh"

#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/result.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <queue>
#include <tuple>
#include <utility>

namespace sc = boost::statechart;

namespace Bridge {
namespace Main {

using CardServer::PeerEntry;
using Engine::CardManager;
using Messaging::CurveKeys;
using Messaging::failure;
using Messaging::Identity;
using Messaging::JsonSerializer;
using Messaging::Reply;
using Messaging::success;

using Impl = CardServerProxy::Impl;
using PositionVector = CardProtocol::PositionVector;
using ShufflingState = CardManager::ShufflingState;
using CardRevealState = Hand::CardRevealState;
using CardTypeVector = std::vector<std::optional<CardType>>;
using CardVector = std::vector<RevealableCard>;
using IndexVector = std::vector<int>;

namespace {

class Initializing;
class Idle;
class WaitingShuffleReply;
class ShuffleCompleted;

template<typename EventType>
struct DrawEventBase : public sc::event<EventType> {
    explicit DrawEventBase(const CardTypeVector& cards) :
        cards {cards}
    {
    }

    const CardTypeVector& cards;
};

struct AcceptPeerEvent : public sc::event<AcceptPeerEvent> {
    AcceptPeerEvent(
        CardProtocol::PositionVector& positions,
        std::string& cardServerPeerEndpoint,
        std::optional<Blob>& cardServerKey,
        bool& ret) :
        positions {positions},
        cardServerPeerEndpoint {cardServerPeerEndpoint},
        cardServerKey {cardServerKey},
        ret {ret}
    {
    }

    CardProtocol::PositionVector& positions;
    std::string& cardServerPeerEndpoint;
    std::optional<Blob>& cardServerKey;
    bool& ret;
};
struct InitializeEvent : public sc::event<InitializeEvent> {};
struct RequestShuffleEvent : public sc::event<RequestShuffleEvent> {};
struct ShuffleSuccessfulEvent : public sc::event<ShuffleSuccessfulEvent> {};
struct RevealSuccessfulEvent : public sc::event<RevealSuccessfulEvent> {};
struct DrawSuccessfulEvent : public DrawEventBase<DrawSuccessfulEvent> {
    using DrawEventBase<DrawSuccessfulEvent>::DrawEventBase;
};
struct NotifyShuffleCompletedEvent :
    public sc::event<NotifyShuffleCompletedEvent> {};
struct RequestRevealEvent : public sc::event<RequestRevealEvent> {
    RequestRevealEvent(
        const std::shared_ptr<BasicHand>& hand, const IndexVector& handNs,
        const IndexVector& deckNs) :
        hand {std::move(hand)},
        handNs {handNs},
        deckNs {deckNs}
    {
    }

    const std::shared_ptr<BasicHand>& hand;
    const IndexVector& handNs;
    const IndexVector& deckNs;
};
struct RevealAllSuccessfulEvent :
    public DrawEventBase<RevealAllSuccessfulEvent> {
    using DrawEventBase<RevealAllSuccessfulEvent>::DrawEventBase;
};

struct PeerRecord {
    PeerRecord(bool isSelf, PositionVector positions) :
        isSelf {isSelf},
        positions {std::move(positions)}
    {
    }

    bool isSelf;
    PositionVector positions;
};

}

class CardServerProxy::Impl :
    public sc::state_machine<Impl, Initializing>, public CardManager {
public:

    Impl(
        Messaging::MessageContext& context, const CurveKeys* keys,
        std::string_view controlEndpoint);

    bool acceptPeer(
        const Identity& identity, PositionVector positions,
        std::string endpoint, std::optional<Blob> serverKey);
    void initializeProtocol();

    template<typename Event, typename ParamIterator>
    void enqueueIfHasCardsParam(
        ParamIterator first, ParamIterator last);

    template<typename... Args>
    void sendCommand(Args&&... args);

    void doRequestShuffle(const RequestShuffleEvent&);
    void doNotifyShuffleCompleted(const NotifyShuffleCompletedEvent&);

    template<typename... Args>
    void emplacePeer(Args&&... args);

    template<typename CardIterator>
    bool revealCards(CardIterator first, CardIterator last);

    auto getSockets() const;
    auto getNumberOfPeers() const;

    template<typename IndexIterator>
    auto cardIterator(IndexIterator iter) const;

    FunctionQueue functionQueue;

private:

    void handleSubscribe(
        std::weak_ptr<Observer<ShufflingState>> observer) override;
    void handleRequestShuffle() override;
    std::shared_ptr<Hand> handleGetHand(const IndexVector& handNs) override;
    bool handleIsShuffleCompleted() const override;
    int handleGetNumberOfCards() const override;
    const Card& handleGetCard(int n) const override;

    void internalHandleHandRevealRequest(
        const std::shared_ptr<BasicHand>& hand,
        const IndexVector& deckNs, const IndexVector& handNs);
    void internalHandleCardServerMessage(Messaging::Socket& socket);

    const SocketVector sockets;
    Messaging::Socket& controlSocket;
    std::vector<PeerRecord> peerRecords;
    CardVector cards;
    Observable<ShufflingState> shufflingStateNotifier;
    std::vector<std::shared_ptr<Hand::CardRevealStateObserver>> handObservers;
};

Impl::Impl(
    Messaging::MessageContext& context, const CurveKeys* const keys,
    const std::string_view controlEndpoint) :
    sockets {{
        {
            Messaging::makeSharedSocket(context, Messaging::SocketType::dealer),
            [this](Messaging::Socket& socket)
            {
                internalHandleCardServerMessage(socket);
                return true;
            }
        }
    }},
    controlSocket {*sockets.front().first}
{
    Messaging::setupCurveClient(
        controlSocket, keys, keys ? keys->publicKey : ByteSpan {});
    Messaging::connectSocket(controlSocket, controlEndpoint);
}

bool Impl::acceptPeer(
    const Identity&, PositionVector positions,
    std::string cardServerPeerEndpoint, std::optional<Blob> serverKey)
{
    auto ret = true;
    functionQueue(
        [this, &positions, &cardServerPeerEndpoint, &serverKey, &ret]()
        {
            process_event(
                AcceptPeerEvent {
                    positions, cardServerPeerEndpoint, serverKey, ret });

        });
    return ret;
}

void Impl::initializeProtocol()
{
    functionQueue([this]() { process_event(InitializeEvent {}); });
}

template<typename Event, typename ParamIterator>
void Impl::enqueueIfHasCardsParam(ParamIterator first, ParamIterator last)
{
    auto&& cards = Messaging::deserializeParam<CardTypeVector>(
        JsonSerializer {}, first, last, CardServer::CARDS_COMMAND);
    if (cards) {
        functionQueue(
            [this, cards = std::move(*cards)]()
            {
                process_event(Event {cards});
            });
    }
}

template<typename... Args>
void Impl::sendCommand(Args&&... args)
{
    Messaging::sendCommandMessage(
        controlSocket, JsonSerializer {}, std::forward<Args>(args)...);
}

void Impl::doRequestShuffle(const RequestShuffleEvent&)
{
    cards.assign(N_CARDS, {});
    handObservers.clear();
    shufflingStateNotifier.notifyAll(ShufflingState::REQUESTED);
    log(LogLevel::DEBUG, "Card server proxy: Shuffling");
    // Command is also used as tag
    sendCommand(CardServer::SHUFFLE_COMMAND, CardServer::SHUFFLE_COMMAND);
    // For each peer, reveal their cards. For self draw cards.
    for (const auto& [order, peer] : enumerate(peerRecords)) {
        auto deckNs = cardsFor(
            peer.positions.begin(), peer.positions.end());
        if (peer.isSelf) {
            log(LogLevel::DEBUG, "Card server proxy: Drawing cards");
            sendCommand(
                CardServer::DRAW_COMMAND, CardServer::DRAW_COMMAND,
                std::pair {CardServer::CARDS_COMMAND, std::move(deckNs)});
        } else {
            log(LogLevel::DEBUG, "Card server proxy: Revealing cards to %s",
                order);
            sendCommand(
                CardServer::REVEAL_COMMAND, CardServer::REVEAL_COMMAND,
                std::pair {CardServer::ORDER_COMMAND, order},
                std::pair {CardServer::CARDS_COMMAND, std::move(deckNs)});
        }
    }
}

void Impl::doNotifyShuffleCompleted(const NotifyShuffleCompletedEvent&)
{
    shufflingStateNotifier.notifyAll(ShufflingState::COMPLETED);
}

template<typename... Args>
void Impl::emplacePeer(Args&&... args)
{
    peerRecords.emplace_back(std::forward<Args>(args)...);
}

template<typename CardIterator>
bool Impl::revealCards(CardIterator first, CardIterator last)
{
    auto n = 0;
    for (; first != last; ++first, ++n) {
        if (n == N_CARDS) {
            return false;
        }
        if (*first) {
            cards[n].reveal(**first);
        }
    }
    return n == N_CARDS;
}

auto Impl::getSockets() const
{
    return sockets;
}

auto Impl::getNumberOfPeers() const
{
    return ssize(peerRecords);
}

template<typename IndexIterator>
auto Impl::cardIterator(IndexIterator iter) const
{
    return containerAccessIterator(iter, cards);
}

void Impl::handleSubscribe(
    std::weak_ptr<Observer<ShufflingState>> observer)
{
    shufflingStateNotifier.subscribe(std::move(observer));
}

void Impl::handleRequestShuffle()
{
    functionQueue([this]() { process_event(RequestShuffleEvent {}); });
}

std::shared_ptr<Hand> Impl::handleGetHand(const IndexVector& deckNs)
{
    auto hand = std::make_shared<BasicHand>(
        containerAccessIterator(deckNs.begin(), cards),
        containerAccessIterator(deckNs.end(), cards));
    handObservers.emplace_back(
        makeObserver<Hand::CardRevealState, IndexVector>(
            [this, hand, deckNs](const auto state, const auto& handNs)
            {
                if (state == Hand::CardRevealState::REQUESTED) {
                    this->internalHandleHandRevealRequest(hand, deckNs, handNs);
                }
            }));
    hand->subscribe(handObservers.back());
    return hand;
}

bool Impl::handleIsShuffleCompleted() const
{
    return state_cast<const ShuffleCompleted*>() != nullptr;
}

int Impl::handleGetNumberOfCards() const
{
    return N_CARDS;
}

const Card& Impl::handleGetCard(const int n) const
{
    assert(n < ssize(cards));
    return cards[n];
}

void Impl::internalHandleHandRevealRequest(
    const std::shared_ptr<BasicHand>& hand,
    const IndexVector& deckNs, const IndexVector& handNs)
{
    // This observer receives notifications about card reveal request and
    // generates event for the state machine. The request contains:
    // deckNs - the indices of the cards in the "deck indexing" (for the card
    // server)
    // handNs - the indices of cards in the "hand indexing" (for the Hand
    // subscribers)
    auto cardInDeckNs = IndexVector(
        containerAccessIterator(handNs.begin(), deckNs),
        containerAccessIterator(handNs.end(), deckNs));
    functionQueue(
        [this, hand, handNs, cardInDeckNs = std::move(cardInDeckNs)]()
        {
            this->process_event(
                RequestRevealEvent {hand, handNs, cardInDeckNs});
        });
}

void Impl::internalHandleCardServerMessage(Messaging::Socket& socket)
{
    using namespace Messaging;

    if (&socket == sockets.front().first.get()) {
        auto reply = std::vector<Messaging::Message> {};
        recvMultipart(socket, std::back_inserter(reply));
        if (reply.size() < 1) {
            log(LogLevel::WARNING,
                "Empty reply received from card server. "
                "This should not be possible with DEALER socket. "
                "Unable to proceed.");
            return;
        }
        const auto tag_iter = reply.begin() + 1;
        if (tag_iter == reply.end()) {
            log(LogLevel::WARNING,
                "Card server failure. "
                "It is unlikely that the protocol can proceed.");
            return;
        }
        const auto status_iter = tag_iter + 1;
        if (status_iter == reply.end() ||
            !isSuccessful(messageView(*status_iter))) {
            log(LogLevel::WARNING,
                "Card server failure. "
                "It is unlikely that the protocol can proceed.");
            return;
        }
        const auto tag_view = messageView(*tag_iter);
        if (tag_view == asBytes(CardServer::SHUFFLE_COMMAND)) {
            functionQueue(
                [this]() { process_event(ShuffleSuccessfulEvent {}); });
        } else if (tag_view == asBytes(CardServer::REVEAL_COMMAND)) {
            functionQueue(
                [this]() { process_event(RevealSuccessfulEvent {}); });
        } else if (tag_view == asBytes(CardServer::DRAW_COMMAND)) {
            enqueueIfHasCardsParam<DrawSuccessfulEvent>(
                status_iter + 1, reply.end());
        } else if (tag_view == asBytes(CardServer::REVEAL_ALL_COMMAND)) {
            enqueueIfHasCardsParam<RevealAllSuccessfulEvent>(
                status_iter + 1, reply.end());
        }
    }
}

namespace {

////////////////////////////////////////////////////////////////////////////////
// Initializing
////////////////////////////////////////////////////////////////////////////////

class Initializing : public sc::simple_state<Initializing, Impl> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<AcceptPeerEvent>,
        sc::custom_reaction<InitializeEvent>>;

    Initializing();

    sc::result react(const AcceptPeerEvent&);
    sc::result react(const InitializeEvent&);

private:
    bool internalAddPeer(
        PositionVector positions,
        std::string&& endpoint, std::optional<Blob>&& serverKey);
    void internalInitCardServer();

    PositionVector selfPositions;
    std::vector<std::pair<PositionVector, PeerEntry>> peers;
};

Initializing::Initializing() :
    selfPositions(Position::begin(), Position::end())
{
}

sc::result Initializing::react(const AcceptPeerEvent& event)
{
    event.ret = internalAddPeer(
        std::move(event.positions),
        std::move(event.cardServerPeerEndpoint),
        std::move(event.cardServerKey));
    return discard_event();
}

sc::result Initializing::react(const InitializeEvent&)
{
    internalInitCardServer();
    return transit<Idle>();
}

bool Initializing::internalAddPeer(
    PositionVector positions, std::string&& endpoint,
    std::optional<Blob>&& serverKey)
{
    if (!positions.empty()) {
        std::sort(positions.begin(), positions.end());
        for (const auto position : positions) {
            const auto iter = std::remove(
                selfPositions.begin(), selfPositions.end(), position);
            selfPositions.erase(iter, selfPositions.end());
        }
        peers.emplace_back(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(positions)),
            std::forward_as_tuple(
                std::move(endpoint), std::move(serverKey)));
        return true;
    }
    return false;
}

void Initializing::internalInitCardServer()
{
    // Sort by first position
    std::sort(
        this->peers.begin(), this->peers.end(),
        [](const auto& peer1, const auto& peer2)
        {
            assert(!peer1.first.empty() && !peer2.first.empty());
            return peer1.first.front() < peer2.first.front();
        });
    // Initialize peer entry vector to be sent to the card server -- also
    // determine the order of self
    auto order = 0;
    auto peers = std::vector<PeerEntry> {};
    for (auto&& peer : this->peers) {
        if (peer.first < selfPositions) {
            ++order;
        }
        peers.emplace_back(std::move(peer.second));
    }
    // Send initialization and the initial shuffle commands
    auto& impl = outermost_context();
    log(LogLevel::DEBUG,
        "Card server proxy: Initializing card server. Order: %d.", order);
    // Command is also used as tag
    impl.sendCommand(
        CardServer::INIT_COMMAND, CardServer::INIT_COMMAND,
        std::pair {CardServer::ORDER_COMMAND, order},
        std::pair {CardServer::PEERS_COMMAND, std::move(peers)});
    // Record peers and positions for the future use
    for (const auto n : to(ssize(this->peers) + 1)) {
        if (n == order) {
            impl.emplacePeer(true, std::move(selfPositions));
        } else {
            const auto n2 = n - (n > order);
            auto&& positions = this->peers[n2].first;
            impl.emplacePeer(false, std::move(positions));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Idle
////////////////////////////////////////////////////////////////////////////////

class Idle : public sc::simple_state<Idle, Impl> {
public:
    using reactions = sc::transition<
        RequestShuffleEvent, WaitingShuffleReply,
        Impl, &Impl::doRequestShuffle>;
};

////////////////////////////////////////////////////////////////////////////////
// WaitingShuffleReply
////////////////////////////////////////////////////////////////////////////////

class WaitingShuffleReply : public sc::simple_state<WaitingShuffleReply, Impl> {
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<ShuffleSuccessfulEvent>,
        sc::custom_reaction<RevealSuccessfulEvent>,
        sc::custom_reaction<DrawSuccessfulEvent>>;

    sc::result react(const ShuffleSuccessfulEvent&);
    sc::result react(const RevealSuccessfulEvent&);
    sc::result react(const DrawSuccessfulEvent&);

private:

    sc::result shuffleCompletedIfReady();

    bool shuffleSuccessful {false};
    bool drawSuccessful {false};
    int revealSuccessfulCount {0};
};

sc::result WaitingShuffleReply::react(const ShuffleSuccessfulEvent&)
{
    shuffleSuccessful = true;
    return shuffleCompletedIfReady();
}

sc::result WaitingShuffleReply::react(const RevealSuccessfulEvent&)
{
    ++revealSuccessfulCount;
    return shuffleCompletedIfReady();
}

sc::result WaitingShuffleReply::react(const DrawSuccessfulEvent& event)
{
    drawSuccessful = true;
    outermost_context().revealCards(event.cards.begin(), event.cards.end());
    return shuffleCompletedIfReady();
}

sc::result WaitingShuffleReply::shuffleCompletedIfReady()
{
    if (shuffleSuccessful && drawSuccessful &&
        revealSuccessfulCount == outermost_context().getNumberOfPeers() - 1)
    {
        post_event(NotifyShuffleCompletedEvent {});
        return transit<ShuffleCompleted>();
    }
    return discard_event();
}

////////////////////////////////////////////////////////////////////////////////
// ShuffleCompleted
////////////////////////////////////////////////////////////////////////////////

class ShuffleCompleted : public sc::simple_state<ShuffleCompleted, Impl> {
public:
    using reactions = boost::mpl::list<
        sc::in_state_reaction<
            NotifyShuffleCompletedEvent, Impl, &Impl::doNotifyShuffleCompleted>,
        sc::custom_reaction<RequestRevealEvent>,
        sc::custom_reaction<RevealAllSuccessfulEvent>,
        sc::transition<
            RequestShuffleEvent, WaitingShuffleReply,
            Impl, &Impl::doRequestShuffle>>;

    sc::result react(const RequestRevealEvent&);
    sc::result react(const RevealAllSuccessfulEvent&);

private:

    struct RequestInfo {
        std::shared_ptr<BasicHand> hand;
        IndexVector handNs;
    };

    std::queue<RequestInfo> requestQueue;
};

sc::result ShuffleCompleted::react(const RequestRevealEvent& event)
{
    log(LogLevel::DEBUG,"Card server proxy: Revealing card(s) to all players");
    // Command is also used as tag
    outermost_context().sendCommand(
        CardServer::REVEAL_ALL_COMMAND, CardServer::REVEAL_ALL_COMMAND,
        std::make_pair(CardServer::CARDS_COMMAND, std::cref(event.deckNs)));
    assert(event.hand);
    requestQueue.emplace(RequestInfo {event.hand, event.handNs});
    return discard_event();
}

sc::result ShuffleCompleted::react(const RevealAllSuccessfulEvent& event)
{
    if (!requestQueue.empty()) {
        outermost_context().revealCards(event.cards.begin(), event.cards.end());
        const auto request = std::move(requestQueue.front());
        requestQueue.pop();
        request.hand->reveal(request.handNs.begin(), request.handNs.end());
    } else {
        log(LogLevel::WARNING,
            "Card server proxy request queue unexpectedly empty");
    }
    return discard_event();
}

}

CardServerProxy::CardServerProxy(
    Messaging::MessageContext& context, const CurveKeys* const keys,
    const std::string_view controlEndpoint) :
    impl {std::make_shared<Impl>(context, keys, controlEndpoint)}
{
    impl->initiate();
}

bool CardServerProxy::handleAcceptPeer(
    const Identity& identity, const PositionVector& positions,
    const OptionalArgs& args)
{
    if (args) {
        const auto cs_iter = args->find(CARD_SERVER_COMMAND);
        if (cs_iter != args->end()) {
            const auto endpoint_iter = cs_iter->find(ENDPOINT_COMMAND);
            const auto key_iter = cs_iter->find(SERVER_KEY_COMMAND);
            if (endpoint_iter != cs_iter->end() && endpoint_iter->is_string() &&
                (key_iter == cs_iter->end() || key_iter->is_string())) {
                assert(impl);
                return impl->acceptPeer(
                    identity, positions, *endpoint_iter,
                    key_iter == cs_iter->end() ?
                    std::nullopt : std::optional {key_iter->get<Blob>()});
            }
        }
    }
    return false;
}

void CardServerProxy::handleInitialize()
{
    assert(impl);
    impl->initializeProtocol();
}

std::shared_ptr<Messaging::MessageHandler>
CardServerProxy::handleGetDealMessageHandler()
{
    return nullptr;
}

CardServerProxy::SocketVector CardServerProxy::handleGetSockets()
{
    assert(impl);
    return impl->getSockets();
}

std::shared_ptr<CardManager> CardServerProxy::handleGetCardManager()
{
    return impl;
}

}
}
