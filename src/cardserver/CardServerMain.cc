#include "cardserver/CardServerMain.hh"

#include "bridge/BridgeConstants.hh"
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

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace Bridge {

namespace CardServer {
using PeerEntry = std::pair<std::string, std::string>;
using PeerVector = std::vector<boost::optional<PeerEntry>>;
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
const std::string CardServerMain::TERMINATE_COMMAND {"terminate"};

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

    TMCG(zmq::context_t& context, PeerVector&& peers);

    bool shuffle();

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

    std::vector<boost::optional<PeerStreamEntry>> peers;
    SchindelhauerTMCG tmcg;
    boost::optional<BarnettSmartVTMF_dlog> vtmf;
    TMCG_Stack<VTMF_Card> stack;
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
    vtmf {}
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
            tmcg.TMCG_CreateStackSecret(secret, false, stack.size(), p_vtmf);
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
        std::swap(stack, stack2);
    }

    std::swap(this->stack, stack);
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
                makeMessageHandler(*this, &Impl::init, JsonSerializer {})
            },
            {
                SHUFFLE_COMMAND,
                makeMessageHandler(*this, &Impl::shuffle, JsonSerializer {})
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
    if (tmcg) {
        if (tmcg->shuffle()) {
            return success();
        }
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
