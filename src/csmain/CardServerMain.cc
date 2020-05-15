// LibTMCG code snippets are based on examples in The LibTMCG Reference Manual
// Copyright Heiko Stamer 2015
// (http://www.nongnu.org/libtmcg/libTMCG.html/index.html)

#include "csmain/CardServerMain.hh"

#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/BridgeConstants.hh"
#include "cardserver/Commands.hh"
#include "cardserver/PeerEntry.hh"
#include "csmain/PeerSocketProxy.hh"
#include "coroutines/AsynchronousExecutionPolicy.hh"
#include "coroutines/Lock.hh"
#include "messaging/Authenticator.hh"
#include "messaging/CardTypeJsonSerializer.hh"
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

#include <boost/format.hpp>
#include <libTMCG.hh>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
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
using Messaging::failure;
using Messaging::Identity;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::Reply;
using Messaging::success;

using PeerVector = std::vector<PeerEntry>;
using SocketVector = PeerSocketProxy::SocketVector;
using IndexVector = std::vector<int>;
using CardVector = std::vector<std::optional<CardType>>;

namespace {

using namespace std::string_literals;
const auto CONTROLLER_USER_ID = "cntrl"s;
const auto PEER_USER_ID_FORMAT = "peer%1%"s;

const auto TMCG_W = 6; // 2^6 = 64, enough to hold all playing cards

struct TMCGInitFailure {};

void failUnless(const bool condition)
{
    if (!condition) {
        throw TMCGInitFailure {};
    }
}

bool isControllingInstance(const Identity& identity)
{
    return identity.userId.empty() || identity.userId == CONTROLLER_USER_ID;
}

class TMCG {
public:

    TMCG(
        AsynchronousExecutionContext& eContext,
        int order, SocketVector peerSockets,
        long unsigned int securityParameter);

    bool shuffle(AsynchronousExecutionContext& eContext);
    bool reveal(
        AsynchronousExecutionContext& eContext, int peerOrder,
        const IndexVector& ns);
    bool revealAll(
        AsynchronousExecutionContext& eContext, const IndexVector& ns);
    bool draw(AsynchronousExecutionContext& eContext, const IndexVector& ns);

    const CardVector& getCards() const;

private:

    bool revealHelper(
        AsynchronousExecutionContext& eContext,
        Messaging::SharedSocket peerSocket, const IndexVector& ns);

    SocketVector peerSockets;
    SchindelhauerTMCG tmcg;
    std::optional<BarnettSmartVTMF_dlog> vtmf;
    TMCG_Stack<VTMF_Card> stack;
    CardVector cards;
};

auto createInputStream(
    Messaging::SharedSocket socket, AsynchronousExecutionContext& eContext)
{
    return Messaging::MessageIStream<AsynchronousExecutionContext> {
        socket, std::ref(eContext)};
}

auto createOutputStream(
    Messaging::SharedSocket socket, AsynchronousExecutionContext& eContext)
{
    return Messaging::MessageOStream<AsynchronousExecutionContext> {
        socket, std::ref(eContext)};
}

TMCG::TMCG(
    AsynchronousExecutionContext& eContext,
    const int order, PeerSocketProxy::SocketVector peerSockets,
    long unsigned int securityParameter) :
    peerSockets(std::move(peerSockets)),
    tmcg {securityParameter, this->peerSockets.size(), TMCG_W},
    vtmf {},
    stack {},
    cards(N_CARDS)
{
    // Phase 1: Generate group

    if (order == 0) {
        vtmf.emplace();
        failUnless(vtmf->CheckGroup());
        for (auto& peerSocket : this->peerSockets) {
            auto outstream = createOutputStream(peerSocket, eContext);
            vtmf->PublishGroup(outstream);
        }
    } else {
        auto instream = createInputStream(this->peerSockets.front(), eContext);
        vtmf.emplace(instream);
        failUnless(vtmf->CheckGroup());
    }

    // Phase 2: Generate keys

    vtmf->KeyGenerationProtocol_GenerateKey();
    for (auto& peerSocket : this->peerSockets) {
        auto outstream = createOutputStream(peerSocket, eContext);
        vtmf->KeyGenerationProtocol_PublishKey(outstream);
    }
    for (auto& peerSocket : this->peerSockets) {
        auto instream = createInputStream(peerSocket, eContext);
        const auto success =
            vtmf->KeyGenerationProtocol_UpdateKey(instream);
        failUnless(success);
    }
    vtmf->KeyGenerationProtocol_Finalize();

    // Insert empty vector to where this peer is in order. Simplifies
    // implementation.
    this->peerSockets.emplace(this->peerSockets.begin() + order);
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

    for (auto& peerSocket : peerSockets) {
        TMCG_Stack<VTMF_Card> stack2;
        if (peerSocket) {
            auto instream = createInputStream(peerSocket, eContext);
            auto outstream = createOutputStream(peerSocket, eContext);
            instream >> stack2;
            if (!instream.good() ||
                !tmcg.TMCG_VerifyStackEquality(
                    stack, stack2, false, p_vtmf, instream, outstream)) {
                log(LogLevel::WARNING, "Failed to verify stack equality.");
                return false;
            }
        } else {
            TMCG_StackSecret<VTMF_CardSecret> secret;
            tmcg.TMCG_CreateStackSecret(secret, false, N_CARDS, p_vtmf);
            tmcg.TMCG_MixStack(stack, stack2, secret, p_vtmf);
            for (auto& peerSocket2 : peerSockets) {
                if (peerSocket2) {
                    auto instream = createInputStream(peerSocket2, eContext);
                    auto outstream = createOutputStream(peerSocket2, eContext);
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
    AsynchronousExecutionContext& eContext, const int peerOrder,
    const IndexVector& ns)
{
    if (0 <= peerOrder && peerOrder < ssize(peerSockets) &&
        peerSockets[peerOrder]) {
        return revealHelper(eContext, peerSockets[peerOrder], ns);
    }
    return false;
}

bool TMCG::revealAll(
    AsynchronousExecutionContext& eContext, const IndexVector& ns)
{
    for (auto& peerSocket : peerSockets) {
        auto success = true;
        if (peerSocket) {
            success = revealHelper(eContext, peerSocket, ns);
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
        if (n < 0 || n >= N_CARDS) {
            return false;
        }
        assert(n < ssize(stack));
        auto& card = stack[n];
        tmcg.TMCG_SelfCardSecret(card, p_vtmf);
        for (auto& peerSocket : peerSockets) {
            if (peerSocket) {
                auto instream = createInputStream(peerSocket, eContext);
                auto outstream = createOutputStream(peerSocket, eContext);
                if (
                    !tmcg.TMCG_VerifyCardSecret(
                        card, p_vtmf, instream, outstream)) {
                    log(LogLevel::WARNING, "Failed to verify card secret.");
                    return false;
                }
            }
        }
        assert(n < ssize(cards));
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
    AsynchronousExecutionContext& eContext,
    Messaging::SharedSocket peerSocket, const IndexVector& ns)
{
    assert(vtmf);
    auto* p_vtmf = &(*vtmf);

    auto instream = createInputStream(peerSocket, eContext);
    auto outstream = createOutputStream(peerSocket, eContext);
    for (const auto n : ns) {
        if (n < 0 || n >= N_CARDS) {
            return false;
        }
        assert(n < ssize(stack));
        tmcg.TMCG_ProveCardSecret(stack[n], p_vtmf, instream, outstream);
    }
    return true;
}

}

class CardServerMain::Impl {
public:
    Impl(
        Messaging::MessageContext& zContext, std::optional<CurveKeys> keys,
        std::string_view controlEndpoint,
        std::string_view peerEndpoint,
        long unsigned int securityParameter);
    void run();

private:

    Reply<> init(
        AsynchronousExecutionContext eContext, const Identity&, int order,
        const PeerVector& peers);
    Reply<> shuffle(AsynchronousExecutionContext eContext, const Identity&);
    Reply<CardVector> draw(
        AsynchronousExecutionContext eContext, const Identity&,
        const IndexVector& ns);
    Reply<> reveal(
        AsynchronousExecutionContext eContext, const Identity&,
        int peerOrder, const IndexVector& ns);
    Reply<CardVector> revealAll(
        AsynchronousExecutionContext eContext, const Identity&,
        const IndexVector& ns);

    void internalAddPeerNodesToAuthenticator(const PeerVector& peers, int order);
    void internalCreatePeerSocketProxy(const PeerVector& peers, int order);

    Messaging::MessageContext& zContext;
    const std::optional<CurveKeys> keys;
    const std::string peerEndpoint;
    const long unsigned int securityParameter;
    std::shared_ptr<Messaging::MessageLoop> messageLoop;
    std::shared_ptr<Messaging::PollingCallbackScheduler> callbackScheduler;
    std::optional<PeerSocketProxy> peerSocketProxy;
    Coroutines::Mutex mutex;
    std::optional<TMCG> tmcg;
    Messaging::MessageQueue messageQueue;
    Messaging::Authenticator authenticator;
    std::vector<Messaging::UserId> authorizedUserIds;
};

CardServerMain::Impl::Impl(
    Messaging::MessageContext& zContext, std::optional<CurveKeys> keys,
    const std::string_view controlEndpoint,
    const std::string_view peerEndpoint,
    long unsigned int securityParameter) :
    zContext {zContext},
    keys {std::move(keys)},
    peerEndpoint {peerEndpoint},
    securityParameter {securityParameter},
    messageLoop {
        std::make_shared<Messaging::MessageLoop>(zContext)},
    callbackScheduler {
        std::make_shared<Messaging::PollingCallbackScheduler>(
            zContext, messageLoop->createTerminationSubscriber())},
    authenticator {
        zContext, messageLoop->createTerminationSubscriber(),
        ([this](const auto& keys) -> Messaging::Authenticator::NodeMap
        {
            if (keys) {
                return {
                    { keys->publicKey, CONTROLLER_USER_ID },
                };
            }
            return {};
        })(this->keys)}
{
    messageQueue.addExecutionPolicy(
        AsynchronousExecutionPolicy {messageLoop, callbackScheduler});
    messageQueue.trySetHandler(
        asBytes(INIT_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::init, JsonSerializer {},
            std::tuple {ORDER_COMMAND, PEERS_COMMAND}));
    messageQueue.trySetHandler(
        asBytes(SHUFFLE_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::shuffle, JsonSerializer {}));
    messageQueue.trySetHandler(
        asBytes(DRAW_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::draw, JsonSerializer {},
            std::tuple {CARDS_COMMAND},
            std::tuple {CARDS_COMMAND}));
    messageQueue.trySetHandler(
        asBytes(REVEAL_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::reveal, JsonSerializer {},
            std::tuple {ORDER_COMMAND, CARDS_COMMAND}));
    messageQueue.trySetHandler(
        asBytes(REVEAL_ALL_COMMAND),
        makeMessageHandler<AsynchronousExecutionPolicy>(
            *this, &Impl::revealAll, JsonSerializer {},
            std::tuple {CARDS_COMMAND},
            std::tuple {CARDS_COMMAND}));
    auto controlSocket = Messaging::makeSharedSocket(
        zContext, Messaging::SocketType::router);
    controlSocket->setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    Messaging::setupCurveServer(*controlSocket, getPtr(this->keys));
    Messaging::bindSocket(*controlSocket, controlEndpoint);
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
    AsynchronousExecutionContext eContext, const Identity& identity,
    const int order, const PeerVector& peers)
{
    log(LogLevel::DEBUG, "Initialization requested by %s", identity);
    if (!isControllingInstance(identity)) {
        return failure();
    }
    Coroutines::Lock lock {eContext, mutex};
    if (!tmcg) {
        internalAddPeerNodesToAuthenticator(peers, order);
        internalCreatePeerSocketProxy(peers, order);
        for (auto&& [socket, callback] : peerSocketProxy->getPollables()) {
            messageLoop->addPollable(std::move(socket), std::move(callback));
        }
        try {
            tmcg.emplace(
                eContext, order, peerSocketProxy->getStreamSockets(),
                securityParameter);
            log(LogLevel::DEBUG, "Initialization success");
            return success();
        } catch (TMCGInitFailure) {
            log(LogLevel::WARNING, "Initialization failed");
            for (auto&& pollable : peerSocketProxy->getPollables()) {
                messageLoop->removePollable(dereference(pollable.first));
            }
            peerSocketProxy.reset();
        }
    }
    return failure();
}

Reply<> CardServerMain::Impl::shuffle(
    AsynchronousExecutionContext eContext, const Identity& identity)
{
    log(LogLevel::DEBUG, "Shuffling requested by %s", identity);
    if (!isControllingInstance(identity)) {
        return failure();
    }
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
    AsynchronousExecutionContext eContext, const Identity& identity,
    const IndexVector& ns)
{
    log(LogLevel::DEBUG,
        "Requested drawing %d cards by %s", ns.size(), identity);
    if (!isControllingInstance(identity)) {
        return failure();
    }
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
    AsynchronousExecutionContext eContext, const Identity& identity,
    const int peerOrder, const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Requested revealing %d cards to %d by %s", ns.size(),
        peerOrder, identity);
    if (!isControllingInstance(identity)) {
        return failure();
    }
    Coroutines::Lock lock {eContext, mutex};
    if (tmcg) {
        if (tmcg->reveal(eContext, peerOrder, ns)) {
            log(LogLevel::DEBUG, "Revealing success");
            return success();
        }
    }
    log(LogLevel::WARNING, "Revealing failed");
    return failure();
}

Reply<CardVector> CardServerMain::Impl::revealAll(
    AsynchronousExecutionContext eContext, const Identity& identity,
    const IndexVector& ns)
{
    log(LogLevel::DEBUG,
        "Requested revealing %d cards to all players by %s",
        ns.size(), identity);
    if (!isControllingInstance(identity)) {
        return failure();
    }
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

void CardServerMain::Impl::internalAddPeerNodesToAuthenticator(
    const PeerVector& peers, const int order)
{
    auto known_peers = Messaging::Authenticator::NodeMap {};
    auto distinct_peer_count = 0;
    authorizedUserIds.resize(peers.size() + 1);
    for (const auto& [n, peer] : enumerate(peers)) {
        if (peer.serverKey) {
            const auto [peer_iter, created] =
                known_peers.try_emplace(*peer.serverKey);
            if (created) {
                const auto user_id =
                    boost::format(PEER_USER_ID_FORMAT) % distinct_peer_count;
                peer_iter->second = user_id.str();
                ++distinct_peer_count;
            }
            const auto peer_order = n + (n >= order);
            assert(0 <= peer_order && peer_order < ssize(authorizedUserIds));
            authorizedUserIds[peer_order] = peer_iter->second;
        }
    }
    for (auto&& [server_key, user_id] : known_peers) {
        authenticator.addNode(std::move(server_key), std::move(user_id));
    }
}

void CardServerMain::Impl::internalCreatePeerSocketProxy(
    const PeerVector& peers, const int order)
{
    auto peerServerSocket = Messaging::Socket {
        zContext, Messaging::SocketType::router};
    peerServerSocket.setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    const auto* curve_keys_ptr = getPtr(keys);
    Messaging::setupCurveServer(peerServerSocket, curve_keys_ptr);
    Messaging::bindSocket(peerServerSocket, peerEndpoint);
    auto peerClientSockets = std::vector<Messaging::Socket> {};
    peerClientSockets.reserve(peers.size());
    for (const auto& peer : peers) {
        auto& socket = peerClientSockets.emplace_back(
            zContext, Messaging::SocketType::dealer);
        Messaging::setupCurveClient(
            socket, getPtr(curve_keys_ptr),
            peer.serverKey ? *peer.serverKey : ByteSpan {});
        Messaging::connectSocket(socket, peer.endpoint);
    }
    peerSocketProxy.emplace(
        zContext, std::move(peerServerSocket), std::move(peerClientSockets),
        order,
        [this](const auto& identity, auto order)
        {
            return (order < ssize(authorizedUserIds)) &&
                (authorizedUserIds[order] == identity.userId);
        });
}

CardServerMain::CardServerMain(
    Messaging::MessageContext& zContext, std::optional<CurveKeys> keys,
    const std::string_view controlEndpoint, const std::string_view peerEndpoint,
    long unsigned int securityParameter) :
    impl {
        std::make_unique<Impl>(
            zContext, std::move(keys), controlEndpoint, peerEndpoint,
            securityParameter)}
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
