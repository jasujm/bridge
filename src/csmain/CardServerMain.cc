// LibTMCG code snippets are based on examples in The LibTMCG Reference Manual
// Copyright Heiko Stamer 2015
// (http://www.nongnu.org/libtmcg/libTMCG.html/index.html)

#include "csmain/CardServerMain.hh"

#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/BridgeConstants.hh"
#include "cardserver/Commands.hh"
#include "cardserver/PeerEntry.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageBuffer.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PeerEntryJsonSerializer.hh"
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
using PeerVector = std::vector<PeerEntry>;
using IndexVector = std::vector<std::size_t>;
using CardVector = std::vector<std::optional<CardType>>;
}

namespace CardServer {

using Messaging::CurveKeys;
using Messaging::EndpointIterator;
using Messaging::failure;
using Messaging::Identity;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::Reply;
using Messaging::success;

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
        zmq::context_t& context, const CurveKeys* keys, int order,
        PeerVector&& peers, EndpointIterator peerEndpointIterator);

    bool shuffle();
    bool reveal(const Identity& identity, const IndexVector& ns);
    bool revealAll(const IndexVector& ns);
    bool draw(const IndexVector& ns);

    const CardVector& getCards() const;

private:

    struct PeerStreamEntry {
        PeerStreamEntry(
            zmq::context_t& context, const CurveKeys* keys, PeerEntry&& entry,
            const EndpointIterator& peerEndpointIterator, int order);

        std::shared_ptr<zmq::socket_t> socket;
        Messaging::MessageBuffer inbuffer;
        Messaging::MessageBuffer outbuffer;
        std::istream instream;
        std::ostream outstream;
        Identity identity;
    };

    bool revealHelper(PeerStreamEntry& peer, const IndexVector& ns);

    std::vector<std::optional<PeerStreamEntry>> peers;
    SchindelhauerTMCG tmcg;
    std::optional<BarnettSmartVTMF_dlog> vtmf;
    TMCG_Stack<VTMF_Card> stack;
    CardVector cards;
};

TMCG::PeerStreamEntry::PeerStreamEntry(
    zmq::context_t& context, const CurveKeys* const keys, PeerEntry&& entry,
    const EndpointIterator& peerEndpointIterator, const int order) :
    socket {std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)},
    inbuffer {socket},
    outbuffer {socket},
    instream(&inbuffer),
    outstream(&outbuffer),
    identity {std::move(entry.identity)}
{
    if (entry.endpoint) {
        Messaging::setupCurveClient(*socket, keys);
        auto endpointIterator = EndpointIterator {*entry.endpoint};
        socket->connect(*(endpointIterator += order));
    } else {
        Messaging::setupCurveServer(*socket, keys);
        socket->bind(*peerEndpointIterator);
    }
}

TMCG::TMCG(
    zmq::context_t& context, const CurveKeys* const keys, const int order,
    PeerVector&& peers, EndpointIterator peerEndpointIterator) :
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
            context, keys, std::move(peer), peerEndpointIterator,
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
            vtmf->PublishGroup((*iter)->outstream);
        }
    } else {
        auto& peer = this->peers.front();
        assert(peer);
        vtmf.emplace(peer->instream);
        failUnless(vtmf->CheckGroup());
    }

    // Phase 3: Generate keys

    vtmf->KeyGenerationProtocol_GenerateKey();
    for (auto& peer : this->peers) {
        if (peer) {
            vtmf->KeyGenerationProtocol_PublishKey(peer->outstream);
        }
    }
    for (auto& peer : this->peers) {
        if (peer) {
            const auto success =
                vtmf->KeyGenerationProtocol_UpdateKey(peer->instream);
            failUnless(success);
        }
    }
    vtmf->KeyGenerationProtocol_Finalize();
}

bool TMCG::shuffle()
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
            peer->instream >> stack2;
            if (!peer->instream.good() ||
                !tmcg.TMCG_VerifyStackEquality(
                    stack, stack2, false, p_vtmf,
                    peer->instream, peer->outstream)) {
                log(LogLevel::WARNING,
                    "Failed to verify stack equality. Prover: %s",
                    formatHex(peer->identity));
                return false;
            }
        } else {
            TMCG_StackSecret<VTMF_CardSecret> secret;
            tmcg.TMCG_CreateStackSecret(secret, false, N_CARDS, p_vtmf);
            tmcg.TMCG_MixStack(stack, stack2, secret, p_vtmf);
            for (auto&& peer2 : peers) {
                if (peer2) {
                    peer2->outstream << stack2 << std::endl;
                    tmcg.TMCG_ProveStackEquality(
                        stack, stack2, secret, false, p_vtmf,
                        peer2->instream, peer2->outstream);
                }
            }
        }
        swap(stack, stack2);
    }

    swap(this->stack, stack);
    std::fill(this->cards.begin(), this->cards.end(), std::nullopt);
    return true;
}

bool TMCG::reveal(const Identity& identity, const IndexVector& ns)
{
    const auto iter = std::find_if(
        peers.begin(), peers.end(),
        [&identity](const auto& peer)
        {
            return peer && peer->identity == identity;
        });
    if (iter != peers.end()) {
        assert(*iter);
        return revealHelper(**iter, ns);
    }
    return false;
}

bool TMCG::revealAll(const IndexVector& ns)
{
    for (auto&& peer : peers) {
        auto success = true;
        if (peer) {
            success = revealHelper(*peer, ns);
        } else {
            success = draw(ns);
        }
        if (!success) {
            return false;
        }
    }
    return true;
}

bool TMCG::draw(const IndexVector& ns)
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
                if (
                    !tmcg.TMCG_VerifyCardSecret(
                        card, p_vtmf, peer->instream, peer->outstream)) {
                    log(LogLevel::WARNING,
                        "Failed to verify card secret. Prover: %s",
                        formatHex(peer->identity));
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

bool TMCG::revealHelper(PeerStreamEntry& peer, const IndexVector& ns)
{
    assert(vtmf);
    auto* p_vtmf = &(*vtmf);

    for (const auto n : ns) {
        if (n >= N_CARDS) {
            return false;
        }
        assert(n < stack.size());
        tmcg.TMCG_ProveCardSecret(
            stack[n], p_vtmf, peer.instream, peer.outstream);
    }
    return true;
}

}

class CardServerMain::Impl {
public:
    Impl(
        zmq::context_t& context, std::optional<CurveKeys> keys,
        const std::string& controlEndpoint,
        const std::string& basePeerEndpoint);
    void run();

private:

    Reply<> init(const Identity&, int order, PeerVector&& peers);
    Reply<> shuffle(const Identity&);
    Reply<CardVector> draw(const Identity&, const IndexVector& ns);
    Reply<> reveal(
        const Identity&, const Identity& identity, const IndexVector& ns);
    Reply<CardVector> revealAll(const Identity&, const IndexVector& ns);

    zmq::context_t& context;
    const std::optional<CurveKeys> keys;
    const EndpointIterator peerEndpointIterator;
    std::optional<TMCG> tmcg;
    Messaging::MessageLoop messageLoop;
};

CardServerMain::Impl::Impl(
    zmq::context_t& context, std::optional<CurveKeys> keys,
    const std::string& controlEndpoint, const std::string& basePeerEndpoint) :
    context {context},
    keys {std::move(keys)},
    peerEndpointIterator {basePeerEndpoint}
{
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::pair);
    Messaging::setupCurveServer(*controlSocket, getPtr(this->keys));
    controlSocket->bind(controlEndpoint);
    messageLoop.addSocket(
        std::move(controlSocket),
        Messaging::MessageQueue {
            {
                {
                    INIT_COMMAND,
                        makeMessageHandler(
                            *this, &Impl::init, JsonSerializer {},
                            std::make_tuple(ORDER_COMMAND, PEERS_COMMAND))
                },
                {
                    SHUFFLE_COMMAND,
                        makeMessageHandler(
                            *this, &Impl::shuffle, JsonSerializer {})
                },
                {
                    DRAW_COMMAND,
                        makeMessageHandler(
                            *this, &Impl::draw, JsonSerializer {},
                            std::make_tuple(CARDS_COMMAND),
                            std::make_tuple(CARDS_COMMAND))
                },
                {
                    REVEAL_COMMAND,
                        makeMessageHandler(
                            *this, &Impl::reveal, JsonSerializer {},
                            std::make_tuple(ID_COMMAND, CARDS_COMMAND))
                },
                {
                    REVEAL_ALL_COMMAND,
                        makeMessageHandler(
                            *this, &Impl::revealAll, JsonSerializer {},
                            std::make_tuple(CARDS_COMMAND),
                            std::make_tuple(CARDS_COMMAND))
                },
            }
        });
}

void CardServerMain::Impl::run()
{
    messageLoop.run();
}

Reply<> CardServerMain::Impl::init(
    const Identity&, const int order, PeerVector&& peers)
{
    log(LogLevel::DEBUG, "Initializing");
    if (!tmcg) {
        try {
            tmcg.emplace(
                context, getPtr(keys), order, std::move(peers),
                peerEndpointIterator);
            return success();
        } catch (TMCGInitFailure) {
            log(LogLevel::WARNING, "Card server initialization failed");
        }
    }
    return failure();
}

Reply<> CardServerMain::Impl::shuffle(const Identity&)
{
    log(LogLevel::DEBUG, "Shuffling requested");
    if (tmcg && tmcg->shuffle()) {
        return success();
    }
    log(LogLevel::WARNING, "Shuffling failed");
    return failure();
}

Reply<CardVector> CardServerMain::Impl::draw(
    const Identity&, const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Drawing %d cards", ns.size());
    if (tmcg && tmcg->draw(ns)) {
        return success(tmcg->getCards());
    }
    log(LogLevel::WARNING, "Drawing failed");
    return failure();
}

Reply<> CardServerMain::Impl::reveal(
    const Identity&, const Identity& identity, const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Revealing %d cards to %s", ns.size(),
        formatHex(identity));
    if (tmcg && tmcg->reveal(identity, ns)) {
        return success();
    }
    log(LogLevel::WARNING, "Revealing failed");
    return failure();
}

Reply<CardVector> CardServerMain::Impl::revealAll(
    const Identity&, const IndexVector& ns)
{
    log(LogLevel::DEBUG, "Revealing %d cards to all players", ns.size());
    if (tmcg && tmcg->revealAll(ns)) {
        return success(tmcg->getCards());
    }
    log(LogLevel::WARNING, "Revealing to all players failed");
    return failure();
}

CardServerMain::CardServerMain(
    zmq::context_t& context, std::optional<CurveKeys> keys,
    const std::string& controlEndpoint, const std::string& basePeerEndpoint) :
    impl {
        std::make_unique<Impl>(
            context, std::move(keys), controlEndpoint, basePeerEndpoint)}
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
