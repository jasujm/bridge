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
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/Replies.hh"
#include "messaging/SerializationUtility.hh"
#include "FunctionObserver.hh"
#include "FunctionQueue.hh"
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
#include <cstddef>
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
using IndexVector = std::vector<std::size_t>;

namespace {

class Initializing;
class Idle;
class WaitingShuffleReply;
class ShuffleCompleted;

template<typename EventType>
struct DrawEventBase : public sc::event<EventType> {
    DrawEventBase(const CardTypeVector& cards) :
        cards {cards}
    {
    }

    const CardTypeVector& cards;
};

struct AcceptPeerEvent : public sc::event<AcceptPeerEvent> {
    AcceptPeerEvent(
        const Identity& identity,
        CardProtocol::PositionVector& positions,
        std::string& cardServerBasePeerEndpoint,
        bool& ret) :
        identity {identity},
        positions {positions},
        cardServerBasePeerEndpoint {cardServerBasePeerEndpoint},
        ret {ret}
    {
    }

    const Identity& identity;
    CardProtocol::PositionVector& positions;
    std::string& cardServerBasePeerEndpoint;
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

struct PeerPosition {
    PeerPosition(
        std::optional<Identity> identity, PositionVector positions) :
        identity {std::move(identity)},
        positions {std::move(positions)}
    {
    }

    std::optional<Identity> identity;
    PositionVector positions;
};

}

class CardServerProxy::Impl :
    public sc::state_machine<Impl, Initializing>, public CardManager {
public:

    Impl(zmq::context_t& context, const std::string& controlEndpoint);

    bool acceptPeer(
        const Identity& identity, PositionVector positions,
        std::string endpoint);
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
    std::size_t handleGetNumberOfCards() const override;

    void internalHandleHandRevealRequest(
        const std::shared_ptr<BasicHand>& hand,
        const IndexVector& deckNs, const IndexVector& handNs);
    void internalHandleCardServerMessage(zmq::socket_t& socket);

    const MessageHandlerVector messageHandlers;
    const SocketVector sockets;
    zmq::socket_t& controlSocket;
    std::vector<PeerPosition> peerPositions;
    CardVector cards;
    Observable<ShufflingState> shufflingStateNotifier;
    std::vector<std::shared_ptr<Hand::CardRevealStateObserver>> handObservers;
};

Impl::Impl(zmq::context_t& context, const std::string& controlEndpoint) :
    sockets {{
        {
            std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair),
            [this](zmq::socket_t& socket)
            {
                internalHandleCardServerMessage(socket);
                return true;
            }
        }
    }},
    controlSocket {*sockets.front().first}
{
    controlSocket.connect(controlEndpoint);
}

bool Impl::acceptPeer(
    const Identity& identity, PositionVector positions,
    std::string cardServerBasePeerEndpoint)
{
    auto ret = true;
    functionQueue(
        [this, &identity, &positions, &cardServerBasePeerEndpoint, &ret]()
        {
            process_event(
                AcceptPeerEvent {
                    identity, positions, cardServerBasePeerEndpoint, ret});
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
    Messaging::sendCommand(
        controlSocket, JsonSerializer {},
        std::forward<Args>(args)...);
}

void Impl::doRequestShuffle(const RequestShuffleEvent&)
{
    cards.assign(N_CARDS, {});
    handObservers.clear();
    shufflingStateNotifier.notifyAll(ShufflingState::REQUESTED);
    log(LogLevel::DEBUG, "Card server proxy: Shuffling");
    sendCommand(CardServer::SHUFFLE_COMMAND);
    // For each peer, reveal their cards. For oneself (indicated by empty
    // identity) draw cards.
    for (const auto& peer : peerPositions) {
        auto deckNs = cardsFor(
            peer.positions.begin(), peer.positions.end());
        if (peer.identity) {
            log(LogLevel::DEBUG, "Card server proxy: Revealing cards to %s",
                asHex(*peer.identity));
            sendCommand(
                CardServer::REVEAL_COMMAND,
                std::tie(CardServer::ID_COMMAND, *peer.identity),
                std::tie(CardServer::CARDS_COMMAND, deckNs));
        } else {
            log(LogLevel::DEBUG, "Card server proxy: Drawing cards");
            sendCommand(
                CardServer::DRAW_COMMAND,
                std::tie(CardServer::CARDS_COMMAND, deckNs));
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
    peerPositions.emplace_back(std::forward<Args>(args)...);
}

template<typename CardIterator>
bool Impl::revealCards(CardIterator first, CardIterator last)
{
    auto n = 0u;
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
    return peerPositions.size();
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

std::size_t Impl::handleGetNumberOfCards() const
{
    return N_CARDS;
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

void Impl::internalHandleCardServerMessage(zmq::socket_t& socket)
{
    using namespace Messaging;

    if (&socket == sockets.front().first.get()) {
        auto reply = std::vector<std::string> {};
        recvAll(std::back_inserter(reply), socket);
        const auto iter = isSuccessfulReply(reply.begin(), reply.end());
        if (iter == reply.end()) {
            log(LogLevel::WARNING,
                "Card server failure."
                "It is unlikely that the protocol can proceed.");
            return;
        }
        log(LogLevel::DEBUG, "Card server proxy: Reply received for %s", *iter);
        if (*iter == CardServer::SHUFFLE_COMMAND) {
            functionQueue(
                [this]() { process_event(ShuffleSuccessfulEvent {}); });
        } else if (*iter == CardServer::REVEAL_COMMAND) {
            functionQueue(
                [this]() { process_event(RevealSuccessfulEvent {}); });
        } else if (*iter == CardServer::DRAW_COMMAND) {
            enqueueIfHasCardsParam<DrawSuccessfulEvent>(iter, reply.end());
        } else if (*iter == CardServer::REVEAL_ALL_COMMAND) {
            enqueueIfHasCardsParam<RevealAllSuccessfulEvent>(iter, reply.end());
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
        const Identity& identity, PositionVector positions,
        std::string&& endpoint);
    void internalInitCardServer();

    PositionVector selfPositions;
    std::vector<std::pair<PositionVector, PeerEntry>> peers;
};

Initializing::Initializing() :
    selfPositions(POSITIONS.begin(), POSITIONS.end())
{
}

sc::result Initializing::react(const AcceptPeerEvent& event)
{
    event.ret = internalAddPeer(
        event.identity, std::move(event.positions),
        std::move(event.cardServerBasePeerEndpoint));
    return discard_event();
}

sc::result Initializing::react(const InitializeEvent&)
{
    internalInitCardServer();
    return transit<Idle>();
}

bool Initializing::internalAddPeer(
    const Identity& identity, PositionVector positions,
    std::string&& endpoint)
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
            std::forward_as_tuple(identity, std::move(endpoint)));
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
    // Only request to connect to those peers that have larger order
    // determined by the first position controlled - also determine the order
    // of self
    auto order = 0u;
    auto peers = std::vector<PeerEntry> {};
    for (auto&& peer : this->peers) {
        auto&& entry = peer.second;
        if (peer.first < selfPositions) {
            peers.emplace_back(std::move(entry));
            ++order;
        } else {
            peers.emplace_back(std::move(entry.identity));
        }
    }
    // Send initialization and the initial shuffle commands
    auto& impl = outermost_context();
    log(LogLevel::DEBUG,
        "Card server proxy: Initializing card server. Order: %d.", order);
    impl.sendCommand(
        CardServer::INIT_COMMAND,
        std::tie(CardServer::ORDER_COMMAND, order),
        std::tie(CardServer::PEERS_COMMAND, peers));
    // Record peers and positions for the future use
    for (const auto n : to(this->peers.size() + 1)) {
        if (n == order) {
            impl.emplacePeer(std::nullopt, std::move(selfPositions));
        } else {
            const auto n2 = n - (n > order);
            auto&& positions = this->peers[n2].first;
            impl.emplacePeer(peers[n2].identity, std::move(positions));
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
    std::size_t revealSuccessfulCount {0};
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
    outermost_context().sendCommand(
        CardServer::REVEAL_ALL_COMMAND,
        std::tie(CardServer::CARDS_COMMAND, event.deckNs));
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
    zmq::context_t& context, const std::string& controlEndpoint) :
    impl {std::make_shared<Impl>(context, controlEndpoint)}
{
    impl->initiate();
}

bool CardServerProxy::handleAcceptPeer(
    const Identity& identity, const PositionVector& positions,
    const OptionalArgs& args)
{
    if (args) {
        const auto iter = args->find(ENDPOINT_COMMAND);
        if (iter != args->end() && iter->is_string()) {
            assert(impl);
            return impl->acceptPeer(identity, positions, *iter);
        }
    }
    return false;
}

void CardServerProxy::handleInitialize()
{
    assert(impl);
    impl->initializeProtocol();
}

CardProtocol::MessageHandlerVector
CardServerProxy::handleGetMessageHandlers()
{
    return {};
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

nlohmann::json makePeerArgsForCardServerProxy(const std::string& endpoint)
{
    return {{ ENDPOINT_COMMAND, endpoint }};
}

}
}
