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
#include "Utility.hh"

#include <boost/optional/optional.hpp>
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
#include <tuple>
#include <utility>

namespace sc = boost::statechart;

namespace Bridge {
namespace Main {

using CardServer::PeerEntry;
using Engine::CardManager;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::Reply;
using Messaging::success;

using Impl = CardServerProxy::Impl;
using PositionVector = CardProtocol::PositionVector;
using ShufflingState = CardManager::ShufflingState;
using CardRevealState = Hand::CardRevealState;
using CardTypeVector = std::vector<boost::optional<CardType>>;
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

struct PeerEvent : public sc::event<PeerEvent> {
    PeerEvent(
        const std::string& identity,
        CardProtocol::PositionVector& positions,
        std::string& cardServerBasePeerEndpoint,
        bool& ret) :
        identity {identity},
        positions {positions},
        cardServerBasePeerEndpoint {cardServerBasePeerEndpoint},
        ret {ret}
    {
    }

    const std::string& identity;
    CardProtocol::PositionVector& positions;
    std::string& cardServerBasePeerEndpoint;
    bool& ret;
};
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
        const std::shared_ptr<BasicHand>& hand, const IndexVector& ns,
        const IndexVector& cardNs) :
        hand {std::move(hand)},
        ns {ns},
        cardNs {cardNs}
    {
    }

    const std::shared_ptr<BasicHand>& hand;
    const IndexVector& ns;
    const IndexVector& cardNs;
};
struct RevealAllSuccessfulEvent :
    public DrawEventBase<RevealAllSuccessfulEvent> {
    using DrawEventBase<RevealAllSuccessfulEvent>::DrawEventBase;
};

struct PeerPosition {
    PeerPosition(
        boost::optional<std::string> identity, PositionVector positions) :
        identity {std::move(identity)},
        positions {std::move(positions)}
    {
    }

    boost::optional<std::string> identity;
    PositionVector positions;
};

}

class CardServerProxy::Impl :
    public sc::state_machine<Impl, Initializing>, public CardManager {
public:

    Impl(zmq::context_t& context, const std::string& controlEndpoint);

    template<typename Event, typename ParamIterator>
    void enqueueIfHasCardsParam(
        ParamIterator first, ParamIterator last);

    template<typename... Args>
    void sendCommand(Args&&... args);

    void doRequestShuffle(const RequestShuffleEvent&);
    void doNotifyShuffleCompleted(const NotifyShuffleCompletedEvent&);

    void setAcceptor(std::weak_ptr<PeerAcceptor> acceptor);

    template<typename... Args>
    void emplacePeer(Args&&... args);

    template<typename CardIterator>
    bool revealCards(CardIterator first, CardIterator last);

    auto getAcceptor() const;
    auto getMessageHandlers() const;
    auto getSockets() const;
    auto getNumberOfPeers() const;

    template<typename IndexIterator>
    auto cardIterator(IndexIterator iter) const;

    FunctionQueue functionQueue;

private:

    Reply<> peer(
        const std::string& identity, PositionVector positions,
        std::string endpoint);

    void handleSubscribe(
        std::weak_ptr<Observer<ShufflingState>> observer) override;
    void handleRequestShuffle() override;
    std::shared_ptr<Hand> handleGetHand(const IndexVector& ns) override;
    bool handleIsShuffleCompleted() const override;
    std::size_t handleGetNumberOfCards() const override;

    void internalHandleHandRevealRequest(
        const std::shared_ptr<BasicHand>& hand,
        const IndexVector& ns, const IndexVector& handNs);
    void internalHandleCardServerMessage(zmq::socket_t& socket);

    const MessageHandlerVector messageHandlers {{
         {
             PEER_COMMAND,
             makeMessageHandler(
                 *this, &Impl::peer, JsonSerializer {},
                 std::make_tuple(POSITIONS_COMMAND, CARD_SERVER_COMMAND))
        },
    }};
    const SocketVector sockets;
    zmq::socket_t& controlSocket;
    std::weak_ptr<PeerAcceptor> peerAcceptor;
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
    controlSocket {*sockets.front().first},
    cards(N_CARDS)
{
    controlSocket.connect(controlEndpoint);
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
    handObservers.clear();
    shufflingStateNotifier.notifyAll(ShufflingState::REQUESTED);
    sendCommand(CardServer::SHUFFLE_COMMAND);
    // For each peer, reveal their cards. For oneself (indicated by empty
    // identity) draw cards.
    for (const auto& peer : peerPositions) {
        auto cardNs = cardsFor(
            peer.positions.begin(), peer.positions.end());
        if (peer.identity) {
            sendCommand(
                CardServer::REVEAL_COMMAND,
                std::tie(CardServer::ID_COMMAND, *peer.identity),
                std::tie(CardServer::CARDS_COMMAND, cardNs));
        } else {
            sendCommand(
                CardServer::DRAW_COMMAND,
                std::tie(CardServer::CARDS_COMMAND, cardNs));
        }
    }
}

void Impl::doNotifyShuffleCompleted(const NotifyShuffleCompletedEvent&)
{
    shufflingStateNotifier.notifyAll(ShufflingState::COMPLETED);
}

void Impl::setAcceptor(std::weak_ptr<PeerAcceptor> acceptor)
{
    peerAcceptor = std::move(acceptor);
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

auto Impl::getAcceptor() const
{
    return std::shared_ptr<PeerAcceptor> {peerAcceptor};
}

auto Impl::getMessageHandlers() const
{
    return messageHandlers;
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

Reply<> Impl::peer(
    const std::string& identity, PositionVector positions,
    std::string cardServerBasePeerEndpoint)
{
    auto ret = true;
    functionQueue(
        [this, &identity, &positions, &cardServerBasePeerEndpoint, &ret]()
        {
            process_event(
                PeerEvent {
                    identity, positions, cardServerBasePeerEndpoint, ret});
        });
    if (ret) {
        return success();
    }
    return failure();
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

std::shared_ptr<Hand> Impl::handleGetHand(const IndexVector& cardNs)
{
    auto hand = std::make_shared<BasicHand>(
        containerAccessIterator(cardNs.begin(), cards),
        containerAccessIterator(cardNs.end(), cards));
    handObservers.emplace_back(
        makeObserver<Hand::CardRevealState, IndexVector>(
            [this, hand, cardNs](const auto state, const auto& handNs)
            {
                if (state == Hand::CardRevealState::REQUESTED) {
                    this->internalHandleHandRevealRequest(hand, cardNs, handNs);
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
    const IndexVector& ns, const IndexVector& handNs)
{
    // This observer receives notifications about card reveal request and
    // generates event for the state machine. The request contains:
    // hands - the indices of cards in "hand indexing"
    // cardNs - the indices of the cards in "card manager indexing"
    auto cardNs = IndexVector(
        containerAccessIterator(handNs.begin(), ns),
        containerAccessIterator(handNs.end(), ns));
    functionQueue(
        [this, hand, handNs, cardNs = std::move(cardNs)]()
        {
            this->process_event(
                RequestRevealEvent {hand, handNs, cardNs});
        });
}

void Impl::internalHandleCardServerMessage(zmq::socket_t& socket)
{
    // FIXME: If any reply is not successful, the protocol just gets
    // stuck. Error handling is needed.
    using namespace Messaging;

    if (&socket == sockets.front().first.get()) {
        auto reply = std::vector<std::string> {};
        recvAll(std::back_inserter(reply), socket);
        if (reply.size() < 2 || reply[0] != REPLY_SUCCESS) {
            return;
        } else if (reply[1] == CardServer::SHUFFLE_COMMAND) {
            functionQueue(
                [this]() { process_event(ShuffleSuccessfulEvent {}); });
        } else if (reply[1] == CardServer::REVEAL_COMMAND) {
            functionQueue(
                [this]() { process_event(RevealSuccessfulEvent {}); });
        } else if (reply[1] == CardServer::DRAW_COMMAND) {
            enqueueIfHasCardsParam<DrawSuccessfulEvent>(
                reply.begin(), reply.end());
        } else if (reply[1] == CardServer::REVEAL_ALL_COMMAND) {
            enqueueIfHasCardsParam<RevealAllSuccessfulEvent>(
                reply.begin(), reply.end());
        }
    }
}

namespace {

////////////////////////////////////////////////////////////////////////////////
// Initializing
////////////////////////////////////////////////////////////////////////////////

class Initializing : public sc::simple_state<Initializing, Impl> {
public:
    using reactions = sc::custom_reaction<PeerEvent>;

    Initializing();

    sc::result react(const PeerEvent&);

private:
    bool internalAddPeer(
        const std::string& identity, PositionVector positions,
        std::string&& endpoint);
    void internalInitCardServer();

    PositionVector selfPositions;
    std::vector<std::pair<PositionVector, PeerEntry>> peers;
};

Initializing::Initializing() :
    selfPositions(POSITIONS.begin(), POSITIONS.end())
{
}

sc::result Initializing::react(const PeerEvent& event)
{
    const auto acceptor = outermost_context().getAcceptor();
    const auto accept_state =
        acceptor->acceptPeer(event.identity, event.positions);
    if (accept_state != CardProtocol::PeerAcceptState::REJECTED) {
        event.ret = true;
        internalAddPeer(
            event.identity, std::move(event.positions),
            std::move(event.cardServerBasePeerEndpoint));
        if (accept_state == CardProtocol::PeerAcceptState::ALL_ACCEPTED) {
            internalInitCardServer();
            return transit<Idle>();
        }
    } else {
        event.ret = false;
    }
    return discard_event();
}

bool Initializing::internalAddPeer(
    const std::string& identity, PositionVector positions,
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
    impl.sendCommand(
        CardServer::INIT_COMMAND,
        std::tie(CardServer::ORDER_COMMAND, order),
        std::tie(CardServer::PEERS_COMMAND, peers));
    // Record peers and positions for the future use
    for (const auto n : to(this->peers.size() + 1)) {
        if (n == order) {
            impl.emplacePeer(boost::none, std::move(selfPositions));
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

    std::shared_ptr<BasicHand> hand;
    IndexVector ns;
};

sc::result ShuffleCompleted::react(const RequestRevealEvent& event)
{
    if (!hand) {
        outermost_context().sendCommand(
            CardServer::REVEAL_ALL_COMMAND,
            std::tie(CardServer::CARDS_COMMAND, event.cardNs));
        assert(event.hand);
        hand = event.hand;
        ns = event.ns;
    }
    return discard_event();
}

sc::result ShuffleCompleted::react(const RevealAllSuccessfulEvent& event)
{
    if (this->hand) {
        outermost_context().revealCards(event.cards.begin(), event.cards.end());
        // Already set this->hand to null if the subscribers request new reveal
        const auto hand = std::move(this->hand);
        hand->reveal(ns.begin(), ns.end());
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

void CardServerProxy::handleSetAcceptor(std::weak_ptr<PeerAcceptor> acceptor)
{
    assert(impl);
    impl->setAcceptor(std::move(acceptor));
}

CardProtocol::MessageHandlerVector
CardServerProxy::handleGetMessageHandlers()
{
    assert(impl);
    return impl->getMessageHandlers();
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
