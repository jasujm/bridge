// LibTMCG code snippets are based on examples in The LibTMCG Reference Manual
// Copyright Heiko Stamer 2015
// (http://www.nongnu.org/libtmcg/libTMCG.html/index.html)

#include "csmain/CardServerMain.hh"

#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/BridgeConstants.hh"
#include "cardserver/Commands.hh"
#include "cardserver/PeerEntry.hh"
#include "coroutines/AsynchronousExecutionPolicy.hh"
#include "coroutines/Lock.hh"
#include "messaging/Authenticator.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageBuffer.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
#include "messaging/PollingCallbackScheduler.hh"
#include "messaging/Security.hh"
#include "Enumerate.hh"
#include "Logging.hh"
#include "HexUtility.hh"
#include "Utility.hh"

#include <libTMCG.hh>
#include <zmq.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

template<typename CardType>
void swap(TMCG_Stack<CardType>& stack1, TMCG_Stack<CardType>& stack2)
{
    stack1.stack.swap(stack2.stack);
}

namespace Bridge {

namespace CardServer {

using Coroutines::AsynchronousExecutionContext;
using Coroutines::AsynchronousExecutionPolicy;
using Messaging::CurveKeys;
using Messaging::EndpointIterator;
using Messaging::failure;
using Messaging::Identity;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::Reply;
using Messaging::success;

using PeerVector = std::vector<PeerEntry>;
using IndexVector = std::vector<std::size_t>;
using CardVector = std::vector<std::optional<CardType>>;

namespace {

// TODO: The security parameter is intentionally low for making testing more
// quick. It should be larger (preferrably adjustable).
const auto SECURITY_PARAMETER = 8;
const auto TMCG_W = 6; // 2^6 = 64, enough to hold all playing cards

struct TMCGInitFailure {};

void failUnless(const bool condition)
{
    if (!condition) {
        throw TMCGInitFailure {};
    }
}

class TMCG {
public:

    TMCG(
        AsynchronousExecutionContext& eContext, zmq::context_t& zContext,
        const CurveKeys* keys, int order, PeerVector&& peers,
        EndpointIterator peerEndpointIterator);

    bool shuffle(AsynchronousExecutionContext& eContext);
    bool reveal(
        AsynchronousExecutionContext& eContext, const Blob& peerId,
        const IndexVector& ns);
    bool revealAll(
        AsynchronousExecutionContext& eContext, const IndexVector& ns);
    bool draw(AsynchronousExecutionContext& eContext, const IndexVector& ns);

    const CardVector& getCards() const;

private:

    struct PeerStreamEntry {
        PeerStreamEntry(
            zmq::context_t& zContext, const CurveKeys* keys, PeerEntry&& entry,
            const EndpointIterator& peerEndpointIterator, int order);

        auto createInputStream(AsynchronousExecutionContext& eContext);
        auto createOutputStream(AsynchronousExecutionContext& eContext);

        std::shared_ptr<zmq::socket_t> socket;
        Blob peerId;
    };

    bool revealHelper(
        AsynchronousExecutionContext& eContext, PeerStreamEntry& peer,
        const IndexVector& ns);

    std::vector<std::optional<PeerStreamEntry>> peers;
    SchindelhauerTMCG tmcg;
    std::optional<BarnettSmartVTMF_dlog> vtmf;
    TMCG_Stack<VTMF_Card> stack;
    CardVector cards;
};

TMCG::PeerStreamEntry::PeerStreamEntry(
    zmq::context_t& zContext, const CurveKeys* const keys, PeerEntry&& entry,
    const EndpointIterator& peerEndpointIterator, const int order) :
    socket {std::make_shared<zmq::socket_t>(zContext, zmq::socket_type::pair)},
    peerId {std::move(entry.id)}
{
    if (entry.endpoint) {
        Messaging::setupCurveClient(
            *socket, keys,
            entry.serverKey ? ByteSpan {*entry.serverKey} : ByteSpan {});
        auto endpointIterator = EndpointIterator {*entry.endpoint};
        socket->connect(*(endpointIterator += order));
    } else {
        Messaging::setupCurveServer(*socket, keys);
        socket->bind(*peerEndpointIterator);
    }
}

auto TMCG::PeerStreamEntry::createInputStream(
    AsynchronousExecutionContext& eContext)
{
    return Messaging::MessageIStream<AsynchronousExecutionContext> {
        socket, std::ref(eContext)};
}

auto TMCG::PeerStreamEntry::createOutputStream(
    AsynchronousExecutionContext& eContext)
{
    return Messaging::MessageOStream<AsynchronousExecutionContext> {
        socket, std::ref(eContext)};
}

TMCG::TMCG(
    AsynchronousExecutionContext& eContext, zmq::context_t& zContext,
    const CurveKeys* const keys, const int order, PeerVector&& peers,
    EndpointIterator peerEndpointIterator) :
    peers(peers.size() + 1),
    tmcg {SECURITY_PARAMETER, peers.size(), TMCG_W},
    vtmf {},
    stack {},
    cards(N_CARDS)
{
    // Phase 1: Connect to all peers

    failUnless(!peers.empty());
    failUnless(0 <= order && static_cast<std::size_t>(order) <= peers.size());
    for (auto&& e : enumerate(peers)) {
        const auto peer_index = e.first + (order <= e.first);
        const auto adjusted_order = order - (order > e.first);
        auto&& peer = e.second;
        assert(static_cast<std::size_t>(peer_index) < this->peers.size());
        this->peers[peer_index].emplace(
            zContext, keys, std::move(peer), peerEndpointIterator,
            adjusted_order);
        ++peerEndpointIterator;
    }

    // Phase 2: Generate group

    if (order == 0) {
        vtmf.emplace();
        failUnless(vtmf->CheckGroup());
        for (
            auto iter = std::next(this->peers.begin());
            iter != this->peers.end(); ++iter) {
            assert(*iter);
            auto outstream = (*iter)->createOutputStream(eContext);
            vtmf->PublishGroup(outstream);
        }
    } else {
        auto& peer = this->peers.front();
        assert(peer);
        auto instream = peer->createInputStream(eContext);
        vtmf.emplace(instream);
        failUnless(vtmf->CheckGroup());
    }

    // Phase 3: Generate keys

    vtmf->KeyGenerationProtocol_GenerateKey();
    for (auto& peer : this->peers) {
        if (peer) {
            auto outstream = peer->createOutputStream(eContext);
            vtmf->KeyGenerationProtocol_PublishKey(outstream);
        }
    }
    for (auto& peer : this->peers) {
        if (peer) {
            auto instream = peer->createInputStream(eContext);
            const auto success =
                vtmf->KeyGenerationProtocol_UpdateKey(instream);
            failUnless(success);
        }
    }
    vtmf->KeyGenerationProtocol_Finalize();
}

bool TMCG::shuffle(AsynchronousExecutionContext& eContext)
{
    assert(vtmf);
    auto* p_vtmf = &(*vtmf);

    TMCG_Stack<VTMF_Card> stack;
    for (const auto type : to(N_CARDS)) {
        VTMF_Card c;
        tmcg.TMCG_CreateOpenCard(c, p_vtmf, type);
        stack.push(c);
    }

    for (auto&& peer : peers) {
        TMCG_Stack<VTMF_Card> stack2;
        if (peer) {
            auto instream = peer->createInputStream(eContext);
            auto outstream = peer->createOutputStream(eContext);
            instream >> stack2;
            if (!instream.good() ||
                !tmcg.TMCG_VerifyStackEquality(
                    stack, stack2, false, p_vtmf, instream, outstream)) {
                log(LogLevel::WARNING,
                    "Failed to verify stack equality. Prover: %s",
                    formatHex(peer->peerId));
                return false;
            }
        } else {
            TMCG_StackSecret<VTMF_CardSecret> secret;
            tmcg.TMCG_CreateStackSecret(secret, false, N_CARDS, p_vtmf);
            tmcg.TMCG_MixStack(stack, stack2, secret, p_vtmf);
            for (auto&& peer2 : peers) {
                if (peer2) {
                    auto instream = peer2->createInputStream(eContext);
                    auto outstream = peer2->createOutputStream(eContext);
                    outstream << stack2 << std::endl;
                    tmcg.TMCG_ProveStackEquality(
                        stack, stack2, secret, false, p_vtmf,
                        instream, outstream);
                }
            }
        }
        swap(stack, stack2);
    }

    swap(this->stack, stack);
    std::fill(this->cards.begin(), this->cards.end(), std::nullopt);
    return true;
}

bool TMCG::reveal(
    AsynchronousExecutionContext& eContext, const Blob& peerId,
    const IndexVector& ns)
{
    const auto iter = std::find_if(
        peers.begin(), peers.end(),
        [&peerId](const auto& peer)
        {
            return peer && peer->peerId == peerId;
        });
    if (iter != peers.end()) {
        assert(*iter);
        return revealHelper(eContext, **iter, ns);
    }
    return false;
}

bool TMCG::revealAll(
    AsynchronousExecutionContext& eContext, const IndexVector& ns)
{
    for (auto&& peer : peers) {
        auto success = true;
        if (peer) {
            success = revealHelper(eContext, *peer, ns);
        } else {
            success = draw(eContext, ns);
        }
        if (!success) {
            return false;
        }
    }
    return true;
}

bool TMCG::draw(AsynchronousExecutionContext& eContext, const IndexVector& ns)
{
    assert(vtmf);
    auto* p_vtmf = &(*vtmf);

    for (const auto n : ns) {
        if (n >= N_CARDS) {
            return false;
        }
        assert(n < stack.size());
        auto& card = stack[n];
        tmcg.TMCG_SelfCardSecret(card, p_vtmf);
        for (auto&& peer : peers) {
            if (peer) {
                auto instream = peer->createInputStream(eContext);
                auto outstream = peer->createOutputStream(eContext);
                if (
                    !tmcg.TMCG_VerifyCardSecret(
                        card, p_vtmf, instream, outstream)) {
                    log(LogLevel::WARNING,
                        "Failed to verify card secret. Prover: %s",
                        formatHex(peer->peerId));
                    return false;
                }
            }
        }
        assert(n < cards.size());
        cards[n].emplace(
            enumerateCardType(tmcg.TMCG_TypeOfCard(card, p_vtmf)));
    }
    return true;
}

const CardVector& TMCG::getCards() const
{
    return cards;
}

bool TMCG::revealHelper(
    AsynchronousExecutionContext& eContext, PeerStreamEntry& peer,
    const IndexVector& ns)
{
    assert(vtmf);
    auto* p_vtmf = &(*vtmf);

    auto instream = peer.createInputStream(eContext);
    auto outstream = peer.createOutputStream(eContext);
    for (const auto n : ns) {
        if (n >= N_CARDS) {
            return false;
        }
        assert(n < stack.size());
        tmcg.TMCG_ProveCardSecret(stack[n], p_vtmf, instream, outstream);
    }
    return true;
}

}

class CardServerMain::Impl {
public:
    Impl(
        zmq::context_t& zContext, std::optional<CurveKeys> keys,
        const std::string& controlEndpoint,
        const std::string& basePeerEndpoint);
    void run();

private:

    Reply<> init(
        AsynchronousExecutionContext eContext, const Identity&, int order,
        PeerVector&& peers);
    Reply<> shuffle(AsynchronousExecutionContext eContext, const Identity&);
    Reply<CardVector> draw(
        AsynchronousExecutionContext eContext, const Identity&,
        const IndexVector& ns);
    Reply<> reveal(
        AsynchronousExecutionContext eContext, const Identity&,
        const Blob& peerId, const IndexVector& ns);
    Reply<CardVector> revealAll(
        AsynchronousExecutionContext eContext, const Identity&,
        const IndexVector& ns);

    zmq::context_t& zContext;
    const std::optional<CurveKeys> keys;
    const EndpointIterator peerEndpointIterator;
    std::shared_ptr<Messaging::MessageLoop> messageLoop;
    std::shared_ptr<Messaging::PollingCallbackScheduler> callbackScheduler;
    Coroutines::Mutex mutex;
    std::optional<TMCG> tmcg;
    Messaging::MessageQueue messageQueue;
    Messaging::Authenticator authenticator;
};

CardServerMain::Impl::Impl(
    zmq::context_t& zContext, std::optional<CurveKeys> keys,
    const std::string& controlEndpoint, const std::string& basePeerEndpoint) :
    zContext {zContext},
    keys {std::move(keys)},
    peerEndpointIterator {basePeerEndpoint},
    messageLoop {
        std::make_shared<Messaging::MessageLoop>(zContext)},
    callbackScheduler {
        std::make_shared<Messaging::PollingCallbackScheduler>(
            zContext, messageLoop->createTerminationSubscriber())},
    authenticator {zContext, messageLoop->createTerminationSubscriber()}
{
    messageQueue.addExecutionPolicy(
        AsynchronousExecutionPolicy {messageLoop, callbackScheduler});
    messageQueue.trySetHandler(
        asBytes(INIT_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::init, JsonSerializer {},
            std::tie(ORDER_COMMAND, PEERS_COMMAND)));
    messageQueue.trySetHandler(
        asBytes(SHUFFLE_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::shuffle, JsonSerializer {}));
    messageQueue.trySetHandler(
        asBytes(DRAW_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::draw, JsonSerializer {},
            std::tie(CARDS_COMMAND),
            std::tie(CARDS_COMMAND)));
    messageQueue.trySetHandler(
        asBytes(REVEAL_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::reveal, JsonSerializer {},
            std::tie(ID_COMMAND, CARDS_COMMAND)));
    messageQueue.trySetHandler(
        asBytes(REVEAL_ALL_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::revealAll, JsonSerializer {},
            std::tie(CARDS_COMMAND),
            std::tie(CARDS_COMMAND)));
    auto controlSocket = std::make_shared<zmq::socket_t>(
        zContext, zmq::socket_type::pair);
    Messaging::setupCurveServer(*controlSocket, getPtr(this->keys));
    controlSocket->bind(controlEndpoint);
    messageLoop->addPollable(std::move(controlSocket), std::ref(messageQueue));
    messageLoop->addPollable(
        callbackScheduler->getSocket(),
        [callbackScheduler = this->callbackScheduler](auto& socket)
        {
            assert(callbackScheduler);
            (*callbackScheduler)(socket);
        });
}

void CardServerMain::Impl::run()
{
    assert(messageLoop);
    messageLoop->run();
}

Reply<> CardServerMain::Impl::init(
    AsynchronousExecutionContext eContext, const Identity&, const int order,
    PeerVector&& peers)
{
    log(LogLevel::DEBUG, "Initialization requested");
    Coroutines::Lock lock {eContext, mutex};
    if (!tmcg) {
        try {
            tmcg.emplace(
                eContext, zContext, getPtr(keys), order, std::move(peers),
                peerEndpointIterator);
            log(LogLevel::DEBUG, "Initialization success");
            return success();
        } catch (TMCGInitFailure) {
            log(LogLevel::WARNING, "Initialization failed");
        }
    }
    return failure();
}

Reply<> CardServerMain::Impl::shuffle(AsynchronousExecutionContext eContext, const Identity&)
{
    log(LogLevel::DEBUG, "Shuffling requested");
    Coroutines::Lock lock {eContext, mutex};
    if (tmcg) {
        if (tmcg->shuffle(eContext)) {
            log(LogLevel::DEBUG, "Shuffling success");
            return success();
        }
    }
    log(LogLevel::WARNING, "Shuffling failed");
    return failure();
}

Reply<CardVector> CardServerMain::Impl::draw(
    AsynchronousExecutionContext eContext, const Identity&,
    const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Requested drawing %d cards", ns.size());
    Coroutines::Lock lock {eContext, mutex};
    if (tmcg) {
        if (tmcg->draw(eContext, ns)) {
            log(LogLevel::DEBUG, "Drawing success");
            return success(tmcg->getCards());
        }
    }
    log(LogLevel::WARNING, "Drawing failed");
    return failure();
}

Reply<> CardServerMain::Impl::reveal(
    AsynchronousExecutionContext eContext, const Identity&, const Blob& peerId,
    const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Requested revealing %d cards to %s", ns.size(),
        formatHex(peerId));
    Coroutines::Lock lock {eContext, mutex};
    if (tmcg) {
        if (tmcg->reveal(eContext, peerId, ns)) {
            log(LogLevel::DEBUG, "Revealing success");
            return success();
        }
    }
    log(LogLevel::WARNING, "Revealing failed");
    return failure();
}

Reply<CardVector> CardServerMain::Impl::revealAll(
    AsynchronousExecutionContext eContext, const Identity&,
    const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Requested revealing %d cards to all players", ns.size());
    Coroutines::Lock lock {eContext, mutex};
    if (tmcg) {
        if (tmcg->revealAll(eContext, ns)) {
            log(LogLevel::DEBUG, "Revealing all success");
            return success(tmcg->getCards());
        }
    }
    log(LogLevel::WARNING, "Revealing to all players failed");
    return failure();
}

CardServerMain::CardServerMain(
    zmq::context_t& zContext, std::optional<CurveKeys> keys,
    const std::string& controlEndpoint, const std::string& basePeerEndpoint) :
    impl {
        std::make_unique<Impl>(
            zContext, std::move(keys), controlEndpoint, basePeerEndpoint)}
{
}

CardServerMain::~CardServerMain() = default;

void CardServerMain::run()
{
    assert(impl);
    impl->run();
}

}
}
