#include "cardserver/CardServerMain.hh"

#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageBuffer.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
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

private:

    struct PeerStreamEntry {
        PeerStreamEntry(
            zmq::context_t& context, PeerEntry&& entry, bool bind);

        static zmq::socket_t initSocket(
            zmq::context_t& context, const PeerEntry& entry, bool bind);

        Messaging::MessageBuffer buffer;
        std::iostream stream;
        std::string identity;
    };

    std::vector<boost::optional<PeerStreamEntry>> peers;
    SchindelhauerTMCG tmcg;
    boost::optional<BarnettSmartVTMF_dlog> vtmf;
};

TMCG::PeerStreamEntry::PeerStreamEntry(
    zmq::context_t& context, PeerEntry&& entry, const bool bind) :
    buffer {initSocket(context, entry, bind)},
    stream {&buffer},
    identity {std::move(entry.first)}
{
}

zmq::socket_t TMCG::PeerStreamEntry::initSocket(
    zmq::context_t& context, const PeerEntry& entry, const bool bind)
{
    zmq::socket_t socket {context, zmq::socket_type::pair};
    if (bind) {
        socket.bind(entry.second);
    } else {
        socket.connect(entry.second);
    }
    return socket;
}

TMCG::TMCG(zmq::context_t& context, PeerVector&& peers) :
    tmcg {SECURITY_PARAMETER, peers.size(), TMCG_W},
    vtmf {},
    peers(peers.size())
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
            vtmf->PublishGroup((*iter)->stream);
        }
    } else {
        auto& peer = this->peers.front();
        assert(peer);
        vtmf.emplace(peer->stream);
        failUnless(vtmf->CheckGroup());
    }

    // Phase 3: Generate keys

    vtmf->KeyGenerationProtocol_GenerateKey();
    for (auto& peer : this->peers) {
        if (peer) {
            vtmf->KeyGenerationProtocol_PublishKey(peer->stream);
        }
    }
    for (auto& peer : this->peers) {
        if (peer) {
            const auto success =
                vtmf->KeyGenerationProtocol_UpdateKey(peer->stream);
            failUnless(success);
        }
    }
    vtmf->KeyGenerationProtocol_Finalize();
}

}

class CardServerMain::Impl {
public:
    Impl(zmq::context_t& context, const std::string& controlEndpoint);
    void run();

private:

    Reply<> init(const std::string& identity, PeerVector&& peers);

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
