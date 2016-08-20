// LibTMCG code snippets are based on examples in The LibTMCG Reference Manual
// Copyright Heiko Stamer 2015
// (http://www.nongnu.org/libtmcg/libTMCG.html/index.html)

#include "cardserver/CardServerMain.hh"

#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/BridgeConstants.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageBuffer.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "Utility.hh"
#include "Zip.hh"

#include <boost/optional/optional.hpp>
#include <libTMCG.hh>
#include <zmq.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
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
using PeerEntry = std::pair<std::string, std::string>;
using PeerVector = std::vector<boost::optional<PeerEntry>>;
using IndexVector = std::vector<std::size_t>;
using CardVector = std::vector<boost::optional<CardType>>;
}

namespace Messaging {
// TODO: Move this to a common place when both serialization and
// deserialization are needed
template<>
struct JsonConverter<CardServer::PeerEntry> {
    static CardServer::PeerEntry convertFromJson(const nlohmann::json& j) {
        return jsonToPair<std::string, std::string>(
            j, "identity", "endpoint");
    }
};
}

namespace CardServer {

using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::Reply;
using Messaging::success;

const std::string CardServerMain::INIT_COMMAND {"init"};
const std::string CardServerMain::SHUFFLE_COMMAND {"shuffle"};
const std::string CardServerMain::DRAW_COMMAND {"draw"};
const std::string CardServerMain::REVEAL_COMMAND {"reveal"};
const std::string CardServerMain::REVEAL_ALL_COMMAND {"revealall"};
const std::string CardServerMain::TERMINATE_COMMAND {"terminate"};

namespace {

// TODO: The security parameter is intentionally low for making testing more
// quick. It should be larger (preferrably adjustable).
const auto SECURITY_PARAMETER = 8;
const auto TMCG_W = 6; // 2^6 = 64, enough to hold all playing cards
const auto CARDS_COMMAND = std::string {"cards"};
const auto PEERS_COMMAND = std::string {"peers"};
const auto ID_COMMAND = std::string {"id"};

struct TMCGInitFailure {};

void failUnless(const bool condition)
{
    if (!condition) {
        throw TMCGInitFailure {};
    }
}

class TMCG {
public:

    TMCG(zmq::context_t& context, PeerVector&& peers);

    bool shuffle();
    bool reveal(const std::string& identity, const IndexVector& ns);
    bool revealAll(const IndexVector& ns);
    bool draw(const IndexVector& ns);

    const CardVector& getCards() const;

private:

    struct PeerStreamEntry {
        PeerStreamEntry(
            zmq::context_t& context, PeerEntry&& entry, bool bind);

        std::shared_ptr<zmq::socket_t> socket;
        Messaging::MessageBuffer inbuffer;
        Messaging::MessageBuffer outbuffer;
        std::istream instream;
        std::ostream outstream;
        std::string identity;
    };

    bool revealHelper(PeerStreamEntry& peer, const IndexVector& ns);

    std::vector<boost::optional<PeerStreamEntry>> peers;
    SchindelhauerTMCG tmcg;
    boost::optional<BarnettSmartVTMF_dlog> vtmf;
    TMCG_Stack<VTMF_Card> stack;
    CardVector cards;
};

TMCG::PeerStreamEntry::PeerStreamEntry(
    zmq::context_t& context, PeerEntry&& entry, const bool bind) :
    socket {std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)},
    inbuffer {socket},
    outbuffer {socket},
    instream(&inbuffer),
    outstream(&outbuffer),
    identity {std::move(entry.first)}
{
    if (bind) {
        socket->bind(entry.second);
    } else {
        socket->connect(entry.second);
    }
}

TMCG::TMCG(zmq::context_t& context, PeerVector&& peers) :
    peers(peers.size()),
    tmcg {SECURITY_PARAMETER, peers.size(), TMCG_W},
    vtmf {},
    stack {},
    cards(N_CARDS)
{
    // Phase 1: Connect to all peers

    auto self_seen = false;
    for (auto&& t : zip(peers, this->peers)) {
        auto&& peer = t.get<0>();
        auto&& this_peer = t.get<1>();
        if (peer) {
            this_peer.emplace(context, std::move(*peer), !self_seen);
        } else {
            failUnless(!self_seen);
            self_seen = true;
        }
    }
    failUnless(self_seen);
    failUnless(!this->peers.empty());

    // Phase 2: Generate group

    if (!peers.front()) { // The first player is the leader
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
    auto p_vtmf = vtmf.get_ptr();
    assert(p_vtmf);

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
    std::fill(this->cards.begin(), this->cards.end(), boost::none);
    return true;
}

bool TMCG::reveal(const std::string& identity, const IndexVector& ns)
{
    auto p_vtmf = vtmf.get_ptr();
    assert(p_vtmf);

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
    auto p_vtmf = vtmf.get_ptr();
    assert(p_vtmf);

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
    auto p_vtmf = vtmf.get_ptr();
    assert(p_vtmf);

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
    auto p_vtmf = vtmf.get_ptr();
    assert(p_vtmf);

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
    Impl(zmq::context_t& context, const std::string& controlEndpoint);
    void run();

private:

    Reply<> init(const std::string&, PeerVector&& peers);
    Reply<> shuffle(const std::string&);
    Reply<CardVector> draw(const std::string&, const IndexVector& ns);
    Reply<> reveal(
        const std::string&, const std::string& identity, const IndexVector& ns);
    Reply<CardVector> revealAll(const std::string&, const IndexVector& ns);

    zmq::context_t& context;
    zmq::socket_t controlSocket;
    Messaging::MessageQueue messageQueue;
    boost::optional<TMCG> tmcg;
};

CardServerMain::Impl::Impl(
    zmq::context_t& context, const std::string& controlEndpoint) :
    context {context},
    controlSocket {context, zmq::socket_type::pair},
    messageQueue {
        {
            {
                INIT_COMMAND,
                makeMessageHandler(
                    *this, &Impl::init, JsonSerializer {}, PEERS_COMMAND)
            },
            {
                SHUFFLE_COMMAND,
                makeMessageHandler(*this, &Impl::shuffle, JsonSerializer {})
            },
            {
                DRAW_COMMAND,
                makeMessageHandler(
                    *this, &Impl::draw, JsonSerializer {}, CARDS_COMMAND)
            },
            {
                REVEAL_COMMAND,
                makeMessageHandler(
                    *this, &Impl::reveal, JsonSerializer {},
                    ID_COMMAND, CARDS_COMMAND)
            },
            {
                REVEAL_ALL_COMMAND,
                makeMessageHandler(
                    *this, &Impl::revealAll, JsonSerializer {}, CARDS_COMMAND)
            },
        },
        TERMINATE_COMMAND
    }
{
    controlSocket.bind(controlEndpoint);
}

void CardServerMain::Impl::run()
{
    while (messageQueue(controlSocket));
}

Reply<> CardServerMain::Impl::init(const std::string&, PeerVector&& peers)
{
    if (!tmcg) {
        try {
            tmcg.emplace(context, std::move(peers));
            return success();
        } catch (TMCGInitFailure) {
            // failed
        }
    }
    return failure();
}

Reply<> CardServerMain::Impl::shuffle(const std::string&)
{
    if (tmcg && tmcg->shuffle()) {
        return success();
    }
    return failure();
}

Reply<CardVector> CardServerMain::Impl::draw(
    const std::string&, const IndexVector& ns)
{
    if (tmcg && tmcg->draw(ns)) {
        return success(tmcg->getCards());
    }
    return failure();
}

Reply<> CardServerMain::Impl::reveal(
    const std::string&, const std::string& identity, const IndexVector& ns)
{
    if (tmcg && tmcg->reveal(identity, ns)) {
        return success();
    }
    return failure();
}

Reply<CardVector> CardServerMain::Impl::revealAll(
    const std::string&, const IndexVector& ns)
{
    if (tmcg && tmcg->revealAll(ns)) {
        return success(tmcg->getCards());
    }
    return failure();
}

CardServerMain::CardServerMain(
    zmq::context_t& context, const std::string& controlEndpoint) :
    impl {std::make_unique<Impl>(context, controlEndpoint)}
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
