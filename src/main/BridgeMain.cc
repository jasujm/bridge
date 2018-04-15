#include "main/BridgeMain.hh"

#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/UuidGenerator.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/BridgeGame.hh"
#include "main/CallbackScheduler.hh"
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

    std::unique_ptr<CardProtocol> makeCardProtocol(
        zmq::context_t& context,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

private:

    Reply<> hello(
        const std::string& identity, const VersionVector& version,
        const std::string& role);
    Reply<Uuid> game(
        const std::string& identity, const boost::optional<Uuid>& uuid,
        const boost::optional<nlohmann::json>& args);
    Reply<Uuid> join(
        const std::string& identity, const boost::optional<Uuid>& gameUuid,
        const boost::optional<Uuid>& playerUuid,
        boost::optional<Position> positions);
    Reply<> call(
        const std::string& identity, const Uuid& gameUuid,
        const boost::optional<Uuid>& playerUuid, const Call& call);
    Reply<> play(
        const std::string& identity, const Uuid& gameUuid,
        const boost::optional<Uuid>& playerUuid,
        const boost::optional<CardType>& card,
        const boost::optional<std::size_t>& index);

    BridgeGame* internalGetGame(const Uuid& gameUuid);
    const Player* internalGetPlayerFor(
        const std::string& identity, const boost::optional<Uuid>& playerUuid);
    void internalInitializePeers(
        zmq::context_t& context, const EndpointVector& peerEndpoints,
        const PositionVector& positions,
        const std::string& cardServerBasePeerEndpoint,
        PeerCommandSender& peerCommandSender);

    UuidGenerator uuidGenerator {createUuidGenerator()};
    bool peerMode;
    std::map<std::string, Role> nodes;
    std::shared_ptr<NodePlayerControl> nodePlayerControl;
    std::shared_ptr<zmq::socket_t> eventSocket;
    std::shared_ptr<Main::CallbackScheduler> callbackScheduler;
    Messaging::MessageQueue messageQueue;
    Messaging::MessageLoop messageLoop;
    std::map<Uuid, BridgeGame> games;
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
    nodePlayerControl {std::make_shared<NodePlayerControl>()},
    eventSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pub)},
    callbackScheduler {std::make_shared<CallbackScheduler>(context)},
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
                    std::make_tuple(GAME_COMMAND, ARGS_COMMAND),
                    std::make_tuple(GAME_COMMAND))
            },
            {
                JOIN_COMMAND,
                makeMessageHandler(
                    *this, &Impl::join, JsonSerializer {},
                    std::make_tuple(
                        GAME_COMMAND, PLAYER_COMMAND, POSITION_COMMAND),
                    std::make_tuple(GAME_COMMAND))
            },
            {
                CALL_COMMAND,
                makeMessageHandler(
                    *this, &Impl::call, JsonSerializer {},
                    std::make_tuple(GAME_COMMAND, PLAYER_COMMAND, CALL_COMMAND))
            },
            {
                PLAY_COMMAND,
                makeMessageHandler(
                    *this, &Impl::play, JsonSerializer {},
                    std::make_tuple(
                        GAME_COMMAND, PLAYER_COMMAND, CARD_COMMAND,
                        INDEX_COMMAND))
            }
        }}
{
    auto endpointIterator = Messaging::EndpointIterator {baseEndpoint};
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    controlSocket->bind(*endpointIterator++);
    eventSocket->bind(*endpointIterator);
    // Default game for peers, if there are any
    if (peerMode) {
        auto peerCommandSender = std::make_shared<PeerCommandSender>();
        auto cardProtocol = makeCardProtocol(
            context, cardServerControlEndpoint, cardServerBasePeerEndpoint,
            peerCommandSender);
        for (auto&& handler : cardProtocol->getMessageHandlers()) {
            messageQueue.trySetHandler(handler.first, handler.second);
        }
        internalInitializePeers(
            context, peerEndpoints, positions, cardServerBasePeerEndpoint,
            *peerCommandSender);
        for (auto&& socket : cardProtocol->getSockets()) {
            messageLoop.addSocket(socket.first, socket.second);
        }
        games.emplace(
            std::piecewise_construct,
            std::make_tuple(Uuid {}),
            std::make_tuple(
                Uuid {}, generatePositionsControlled(positions), eventSocket,
                std::move(cardProtocol), std::move(peerCommandSender),
                callbackScheduler));
    }
    messageQueue.trySetHandler(
        GET_COMMAND,
        std::make_shared<GetMessageHandler>(
            [this](const Uuid& uuid) { return internalGetGame(uuid); },
            nodePlayerControl));
    messageLoop.addSocket(
        callbackScheduler->getSocket(),
        [callbackScheduler = this->callbackScheduler](auto& socket)
        {
            assert(callbackScheduler);
            (*callbackScheduler)(socket);
        }),
    messageLoop.addSocket(
        std::move(controlSocket),
        [&queue = this->messageQueue](auto& socket) { queue(socket); });
}


void BridgeMain::Impl::run()
{
    messageLoop.run();
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

Reply<Uuid> BridgeMain::Impl::game(
    const std::string& identity, const boost::optional<Uuid>& gameUuid,
    const boost::optional<nlohmann::json>& args)
{
    log(LogLevel::DEBUG, "Game command from %s. Game: %s. Args: %s",
        asHex(identity), gameUuid, args);

    const auto iter = nodes.find(identity);
    if (iter != nodes.end()) {
        if (iter->second == Role::PEER && gameUuid && args) {
            const auto game = internalGetGame(*gameUuid);
            if (game && game->addPeer(identity, *args)) {
                return success(*gameUuid);
            }
        } else if (iter->second == Role::CLIENT) {
            const auto uuid_for_game = gameUuid ? *gameUuid : uuidGenerator();
            const auto game = games.emplace(
                std::piecewise_construct,
                std::make_tuple(uuid_for_game),
                std::make_tuple(uuid_for_game, eventSocket, callbackScheduler));
            if (game.second) {
                return success(uuid_for_game);
            }
        }
    }
    return failure();
}

Reply<Uuid> BridgeMain::Impl::join(
    const std::string& identity, const boost::optional<Uuid>& gameUuid,
    const boost::optional<Uuid>& playerUuid, boost::optional<Position> position)
{
    log(LogLevel::DEBUG, "Join command from %s. Game: %s. Player: %s. Position: %s",
        asHex(identity), gameUuid, playerUuid, position);

    const auto iter = nodes.find(identity);
    if (iter != nodes.end()) {
        if (iter->second == Role::PEER && (!playerUuid || !gameUuid)) {
            return failure();
        }
        const auto uuid_for_game = gameUuid.value_or(Uuid {});
        const auto game = internalGetGame(uuid_for_game);
        if (game &&
            (position = game->getPositionForPlayerToJoin(identity, position))) {
            assert(nodePlayerControl);
            if (auto player = nodePlayerControl->createPlayer(identity, playerUuid)) {
                game->join(identity, *position, std::move(player));
                return success(uuid_for_game);
            }
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::call(
    const std::string& identity, const Uuid& gameUuid,
    const boost::optional<Uuid>& playerUuid, const Call& call)
{
    log(LogLevel::DEBUG, "Call command from %s. Game: %s. Player: %s. Call: %s",
        asHex(identity), gameUuid, playerUuid, call);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        const auto game = internalGetGame(gameUuid);
        if (game && game->call(identity, *player, call)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::play(
    const std::string& identity, const Uuid& gameUuid,
    const boost::optional<Uuid>& playerUuid,
    const boost::optional<CardType>& card,
    const boost::optional<std::size_t>& index)
{
    log(LogLevel::DEBUG, "Play command from %s. Game: %s. Player: %s. Card: %s. Index: %d",
        asHex(identity), gameUuid, playerUuid, card, index);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        const auto game = internalGetGame(gameUuid);
        if (game && game->play(identity, *player, card, index)) {
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
    auto args = makePeerArgsForCardServerProxy(cardServerBasePeerEndpoint);
    args[POSITIONS_COMMAND] = Messaging::toJson(positions);
    peerCommandSender.sendCommand(
        JsonSerializer {},
        GAME_COMMAND,
        std::make_pair(std::cref(GAME_COMMAND), Uuid {}),
        std::make_pair(std::cref(ARGS_COMMAND), args));
}

BridgeGame* BridgeMain::Impl::internalGetGame(const Uuid& gameUuid)
{
    const auto iter = games.find(gameUuid);
    if (iter != games.end()) {
        return &iter->second;
    }
    return nullptr;
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

}
}
