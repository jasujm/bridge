#include "main/BridgeMain.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Hand.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "main/BridgeGame.hh"
#include "main/CardServerProxy.hh"
#include "main/Commands.hh"
#include "main/GetMessageHandler.hh"
#include "main/NodePlayerControl.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Logging.hh"

#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <zmq.hpp>

#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::MessageQueue;
using Messaging::Reply;
using Messaging::success;

namespace {

using VersionVector = std::vector<int>;
using PositionSet = std::set<Position>;

const std::string PEER_ROLE {"peer"};
const std::string CLIENT_ROLE {"client"};

enum class Role {
    PEER,
    CLIENT
};

auto generatePositionsControlled(const BridgeMain::PositionVector& positions)
{
    if (positions.empty()) {
        return PositionSet(POSITIONS.begin(), POSITIONS.end());
    }
    return PositionSet(positions.begin(), positions.end());
}

}

class BridgeMain::Impl {
public:

    Impl(
        zmq::context_t& context, const std::string& baseEndpoint,
        PositionVector positions, const EndpointVector& peerEndpoints,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint);

    void run();

    void terminate();

    std::unique_ptr<CardProtocol> makeCardProtocol(
        zmq::context_t& context,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

private:

    Reply<> hello(
        const std::string& identity, const VersionVector& version,
        const std::string& role);
    Reply<> game(
        const std::string& identity, PositionVector positions,
        const boost::optional<nlohmann::json>& args);
    Reply<> join(
        const std::string& identity, const boost::optional<Uuid>& playerUuid,
        boost::optional<Position> positions);
    Reply<> call(
        const std::string& identity, const boost::optional<Uuid>& playerUuid,
        const Call& call);
    Reply<> play(
        const std::string& identity, const boost::optional<Uuid>& playerUuid,
        const boost::optional<CardType>& card,
        const boost::optional<std::size_t>& index);

    const Player* internalGetPlayerFor(
        const std::string& identity, const boost::optional<Uuid>& playerUuid);
    void internalInitializePeers(
        zmq::context_t& context, const EndpointVector& peerEndpoints,
        const PositionVector& positions,
        const std::string& cardServerBasePeerEndpoint,
        PeerCommandSender& peerCommandSender);

    bool peerMode;
    std::map<std::string, Role> nodes;
    std::shared_ptr<NodePlayerControl> nodePlayerControl;
    std::shared_ptr<zmq::socket_t> eventSocket;
    Messaging::MessageQueue messageQueue;
    Messaging::MessageLoop messageLoop;
    std::shared_ptr<BridgeGame> bridgeGame;
};

std::unique_ptr<CardProtocol> BridgeMain::Impl::makeCardProtocol(
    zmq::context_t& context,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint,
    std::shared_ptr<PeerCommandSender> peerCommandSender)
{
    if (cardServerControlEndpoint.empty() &&
        cardServerBasePeerEndpoint.empty()) {
        log(LogLevel::DEBUG, "Card exchange protocol: simple");
        return std::make_unique<SimpleCardProtocol>(peerCommandSender);
    }
    log(LogLevel::DEBUG, "Card exchange protocol: card server");
    return std::make_unique<CardServerProxy>(
        context, cardServerControlEndpoint);
}

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& baseEndpoint,
    PositionVector positions, const EndpointVector& peerEndpoints,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint) :
    peerMode {!peerEndpoints.empty()},
    nodePlayerControl(std::make_shared<NodePlayerControl>()),
    eventSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pub)},
    messageQueue {
        {
            {
                HELLO_COMMAND,
                makeMessageHandler(
                    *this, &Impl::hello, JsonSerializer {},
                    std::make_tuple(VERSION_COMMAND, ROLE_COMMAND))
            },
            {
                GAME_COMMAND,
                makeMessageHandler(
                    *this, &Impl::game, JsonSerializer {},
                    std::make_tuple(POSITIONS_COMMAND, ARGS_COMMAND))
            },
            {
                JOIN_COMMAND,
                makeMessageHandler(
                    *this, &Impl::join, JsonSerializer {},
                    std::make_tuple(PLAYER_COMMAND, POSITION_COMMAND))
            },
            {
                CALL_COMMAND,
                makeMessageHandler(
                    *this, &Impl::call, JsonSerializer {},
                    std::make_tuple(PLAYER_COMMAND, CALL_COMMAND))
            },
            {
                PLAY_COMMAND,
                makeMessageHandler(
                    *this, &Impl::play, JsonSerializer {},
                    std::make_tuple(PLAYER_COMMAND, CARD_COMMAND, INDEX_COMMAND))
            }
        }}
{
    auto endpointIterator = Messaging::EndpointIterator {baseEndpoint};
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    controlSocket->bind(*endpointIterator++);
    eventSocket->bind(*endpointIterator);
    auto peerCommandSender = std::make_shared<PeerCommandSender>();
    auto cardProtocol = makeCardProtocol(
        context, cardServerControlEndpoint, cardServerBasePeerEndpoint,
        peerCommandSender);
    for (auto&& handler : cardProtocol->getMessageHandlers()) {
        messageQueue.trySetHandler(handler.first, handler.second);
    }
    if (peerMode) {
        internalInitializePeers(
            context, peerEndpoints, positions, cardServerBasePeerEndpoint,
            *peerCommandSender);
    }
    for (auto&& socket : cardProtocol->getSockets()) {
        messageLoop.addSocket(socket.first, socket.second);
    }
    bridgeGame = std::make_shared<BridgeGame>(
        generatePositionsControlled(positions), eventSocket,
        std::move(cardProtocol), std::move(peerCommandSender));
    messageQueue.trySetHandler(
        GET_COMMAND,
        std::make_shared<GetMessageHandler>(bridgeGame, nodePlayerControl));
    messageLoop.addSocket(
        std::move(controlSocket),
        [&queue = this->messageQueue](auto& socket) { queue(socket); });
}


void BridgeMain::Impl::run()
{
    messageLoop.run();
}

void BridgeMain::Impl::terminate()
{
    messageLoop.terminate();
}

Reply<> BridgeMain::Impl::hello(
    const std::string& identity, const VersionVector& version,
    const std::string& role)
{
    log(LogLevel::DEBUG, "Hello command from %s. Version: %d. Role: %s",
        asHex(identity), version.empty() ? -1 : version.front(), role);
    if (version.empty() || version.front() > 0 ||
        nodes.find(identity) != nodes.end()) {
        return failure();
    } else if (role == PEER_ROLE && peerMode) {
        log(LogLevel::DEBUG, "Peer accepted: %s", asHex(identity));
        nodes.emplace(identity, Role::PEER);
        return success();
    } else if (role == CLIENT_ROLE) {
        log(LogLevel::DEBUG, "Client accepted: %s", asHex(identity));
        nodes.emplace(identity, Role::CLIENT);
        return success();
    }
    return failure();
}

Reply<> BridgeMain::Impl::game(
    const std::string& identity, PositionVector positions,
    const boost::optional<nlohmann::json>& args)
{
    log(LogLevel::DEBUG, "Game command from %s", asHex(identity));

    const auto iter = nodes.find(identity);
    if (iter != nodes.end() && iter->second == Role::PEER) {
        assert(bridgeGame);
        if (bridgeGame->addPeer(identity, std::move(positions), args)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::join(
    const std::string& identity, const boost::optional<Uuid>& playerUuid,
    boost::optional<Position> position)
{
    log(LogLevel::DEBUG, "Join command from %s. Player: %s. Position: %s",
        asHex(identity), playerUuid, position);

    assert(bridgeGame);
    const auto iter = nodes.find(identity);
    if (iter != nodes.end()) {
        if (iter->second == Role::PEER && !playerUuid) {
            return failure();
        }
        position = bridgeGame->getPositionForPlayerToJoin(identity, position);
        if (position) {
            assert(nodePlayerControl);
            auto player = nodePlayerControl->createPlayer(identity, playerUuid);
            bridgeGame->join(identity, *position, std::move(player));
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::call(
    const std::string& identity, const boost::optional<Uuid>& playerUuid,
    const Call& call)
{
    log(LogLevel::DEBUG, "Call command from %s. Player: %s. Call: %s",
        asHex(identity), playerUuid, call);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        assert(bridgeGame);
        if (bridgeGame->call(identity, *player, call)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::play(
    const std::string& identity, const boost::optional<Uuid>& playerUuid,
    const boost::optional<CardType>& card,
    const boost::optional<std::size_t>& index)
{
    log(LogLevel::DEBUG, "Play command from %s. Player: %s. Card: %s. Index: %d",
        asHex(identity), playerUuid, card, index);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        assert(bridgeGame);
        if (bridgeGame->play(identity, *player, card, index)) {
            return success();
        }
    }
    return failure();
}

void BridgeMain::Impl::internalInitializePeers(
    zmq::context_t& context, const EndpointVector& peerEndpoints,
    const PositionVector& positions,
    const std::string& cardServerBasePeerEndpoint,
    PeerCommandSender& peerCommandSender)
{
    for (const auto& endpoint : peerEndpoints) {
        messageLoop.addSocket(
            peerCommandSender.addPeer(context, endpoint),
            [&sender = peerCommandSender](zmq::socket_t& socket)
            {
                sender.processReply(socket);
                return true;
            });
    }
    peerCommandSender.sendCommand(
        JsonSerializer {},
        HELLO_COMMAND,
        std::make_pair(std::cref(VERSION_COMMAND), VersionVector {0}),
        std::tie(ROLE_COMMAND, PEER_ROLE));
    peerCommandSender.sendCommand(
        JsonSerializer {},
        GAME_COMMAND,
        std::tie(POSITIONS_COMMAND, positions),
        std::make_pair(
            std::cref(ARGS_COMMAND),
            makePeerArgsForCardServerProxy(cardServerBasePeerEndpoint)));
}

const Player* BridgeMain::Impl::internalGetPlayerFor(
    const std::string& identity, const boost::optional<Uuid>& playerUuid)
{
    assert(nodePlayerControl);
    return nodePlayerControl->getPlayer(identity, playerUuid).get();
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& baseEndpoint,
    PositionVector positions, const EndpointVector& peerEndpoints,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint) :
    impl {
        std::make_shared<Impl>(
            context, baseEndpoint, std::move(positions),
            std::move(peerEndpoints), cardServerControlEndpoint,
            cardServerBasePeerEndpoint)}
{
}

BridgeMain::BridgeMain(BridgeMain&&) = default;

BridgeMain::~BridgeMain() = default;

void BridgeMain::run()
{
    assert(impl);
    impl->run();
}

void BridgeMain::terminate()
{
    assert(impl);
    impl->terminate();
}

}
}
