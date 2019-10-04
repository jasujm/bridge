#include "main/BridgeMain.hh"

#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Position.hh"
#include "bridge/UuidGenerator.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/BridgeGame.hh"
#include "main/BridgeGameConfig.hh"
#include "main/CardProtocol.hh"
#include "main/Commands.hh"
#include "main/Config.hh"
#include "main/GetMessageHandler.hh"
#include "main/NodePlayerControl.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/Authenticator.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/DispatchingMessageHandler.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/Identity.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PollingCallbackScheduler.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/Security.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Logging.hh"

#include <boost/uuid/uuid_io.hpp>

#include <iterator>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

using Messaging::failure;
using Messaging::Identity;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::MessageQueue;
using Messaging::Reply;
using Messaging::success;

namespace {

using VersionVector = std::vector<int>;
using GameMessageHandler = Messaging::DispatchingMessageHandler<
    Uuid, JsonSerializer>;

auto initializeGameMessageHandler()
{
    return Messaging::makeDispatchingMessageHandler<Uuid>(
        stringToBlob(GAME_COMMAND), JsonSerializer {});
}

const std::string PEER_ROLE {"peer"};
const std::string CLIENT_ROLE {"client"};

enum class Role {
    PEER,
    CLIENT
};

}

class BridgeMain::Impl {
public:

    Impl(Messaging::MessageContext& context, Config config);

    void run();

private:

    Reply<> hello(
        const Identity& identity, const VersionVector& version,
        const std::string& role);
    Reply<Uuid> game(
        const Identity& identity, const std::optional<Uuid>& uuid,
        const std::optional<nlohmann::json>& args);
    Reply<Uuid> join(
        const Identity& identity, const std::optional<Uuid>& gameUuid,
        const std::optional<Uuid>& playerUuid,
        std::optional<Position> positions);
    Reply<> call(
        const Identity& identity, const Uuid& gameUuid,
        const std::optional<Uuid>& playerUuid, const Call& call);
    Reply<> play(
        const Identity& identity, const Uuid& gameUuid,
        const std::optional<Uuid>& playerUuid,
        const std::optional<CardType>& card,
        const std::optional<int>& index);

    BridgeGame* internalGetGame(const Uuid& gameUuid);
    const Player* internalGetPlayerFor(
        const Identity& identity, const std::optional<Uuid>& playerUuid);

    const Config config;
    UuidGenerator uuidGenerator {createUuidGenerator()};
    std::map<Identity, Role> nodes;
    std::shared_ptr<NodePlayerControl> nodePlayerControl;
    Messaging::SharedSocket eventSocket;
    std::shared_ptr<GameMessageHandler> dealMessageHandler {
        initializeGameMessageHandler()};
    std::shared_ptr<GameMessageHandler> getMessageHandler {
        initializeGameMessageHandler()};
    Messaging::MessageQueue messageQueue;
    Messaging::MessageLoop messageLoop;
    Messaging::Authenticator authenticator;
    std::shared_ptr<Messaging::PollingCallbackScheduler> callbackScheduler;
    std::map<Uuid, BridgeGame> games;
    std::queue<std::pair<Uuid, BridgeGame*>> availableGames;
};

BridgeMain::Impl::Impl(Messaging::MessageContext& context, Config config) :
    config {std::move(config)},
    nodePlayerControl {std::make_shared<NodePlayerControl>()},
    eventSocket {
        Messaging::makeSharedSocket(context, Messaging::SocketType::pub)},
    messageQueue {
        {
            {
                stringToBlob(HELLO_COMMAND),
                makeMessageHandler(
                    *this, &Impl::hello, JsonSerializer {},
                    std::make_tuple(VERSION_COMMAND, ROLE_COMMAND))
            },
            {
                stringToBlob(GAME_COMMAND),
                makeMessageHandler(
                    *this, &Impl::game, JsonSerializer {},
                    std::make_tuple(GAME_COMMAND, ARGS_COMMAND),
                    std::make_tuple(GAME_COMMAND))
            },
            {
                stringToBlob(JOIN_COMMAND),
                makeMessageHandler(
                    *this, &Impl::join, JsonSerializer {},
                    std::make_tuple(
                        GAME_COMMAND, PLAYER_COMMAND, POSITION_COMMAND),
                    std::make_tuple(GAME_COMMAND))
            },
            {
                stringToBlob(CALL_COMMAND),
                makeMessageHandler(
                    *this, &Impl::call, JsonSerializer {},
                    std::make_tuple(GAME_COMMAND, PLAYER_COMMAND, CALL_COMMAND))
            },
            {
                stringToBlob(PLAY_COMMAND),
                makeMessageHandler(
                    *this, &Impl::play, JsonSerializer {},
                    std::make_tuple(
                        GAME_COMMAND, PLAYER_COMMAND, CARD_COMMAND,
                        INDEX_COMMAND))
            },
            {
                stringToBlob(DEAL_COMMAND),
                dealMessageHandler
            },
            {
                stringToBlob(GET_COMMAND),
                getMessageHandler
            }
        }},
    messageLoop {context},
    authenticator {
        context, messageLoop.createTerminationSubscriber(),
        this->config.getKnownPeers()},
    callbackScheduler {
        std::make_shared<Messaging::PollingCallbackScheduler>(
            context, messageLoop.createTerminationSubscriber())}
{
    authenticator.ensureRunning();
    const auto keys = this->config.getCurveConfig();
    auto endpointIterator = this->config.getEndpointIterator();
    auto controlSocket = Messaging::makeSharedSocket(
        context, Messaging::SocketType::router);
    controlSocket->setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    Messaging::setupCurveServer(*controlSocket, keys);
    Messaging::bindSocket(*controlSocket, *endpointIterator++);
    Messaging::setupCurveServer(*eventSocket, keys);
    Messaging::bindSocket(*eventSocket, *endpointIterator++);
    for (auto& gameConfig : this->config.getGameConfigs()) {
        const auto& uuid = gameConfig.uuid;
        const auto emplaced_game = games.emplace(
            uuid,
            gameFromConfig(
                gameConfig, context, keys, eventSocket, callbackScheduler,
                this->config.getKnownPeers()));
        if (emplaced_game.second) {
            auto& game = emplaced_game.first->second;
            availableGames.emplace(uuid, &game);
            if (const auto peer_command_sender = game.getPeerCommandSender()) {
                for (auto&& [socket, cb] : peer_command_sender->getSockets()) {
                    messageLoop.addPollable(std::move(socket), std::move(cb));
                }
            }
            if (const auto card_protocol = game.getCardProtocol()) {
                if (auto&& hdl = card_protocol->getDealMessageHandler()) {
                    dealMessageHandler->trySetDelegate(uuid, std::move(hdl));
                }
                getMessageHandler->trySetDelegate(
                    uuid,
                    std::make_shared<GetMessageHandler>(
                        game.getInfo(), nodePlayerControl));
                for (auto&& [socket, cb] : card_protocol->getSockets()) {
                    messageLoop.addPollable(std::move(socket), std::move(cb));
                }
            }
        }
    }
    messageLoop.addPollable(
        callbackScheduler->getSocket(),
        [callbackScheduler = this->callbackScheduler](auto& socket)
        {
            assert(callbackScheduler);
            (*callbackScheduler)(socket);
        }),
    messageLoop.addPollable(
        std::move(controlSocket),
        [&queue = this->messageQueue](auto& socket) { queue(socket); });
}


void BridgeMain::Impl::run()
{
    messageLoop.run();
}

Reply<> BridgeMain::Impl::hello(
    const Identity& identity, const VersionVector& version,
    const std::string& role)
{
    log(LogLevel::DEBUG, "Hello command from %s. Version: %d. Role: %s",
        identity, version.empty() ? -1 : version.front(), role);
    if (version.empty() || version.front() > 0 ||
        nodes.find(identity) != nodes.end()) {
        return failure();
    } else if (role == PEER_ROLE) {
        log(LogLevel::DEBUG, "Peer accepted: %s", identity);
        nodes.emplace(identity, Role::PEER);
        return success();
    } else if (role == CLIENT_ROLE) {
        log(LogLevel::DEBUG, "Client accepted: %s", identity);
        nodes.emplace(identity, Role::CLIENT);
        return success();
    }
    return failure();
}

Reply<Uuid> BridgeMain::Impl::game(
    const Identity& identity, const std::optional<Uuid>& gameUuid,
    const std::optional<nlohmann::json>& args)
{
    log(LogLevel::DEBUG, "Game command from %s. Game: %s. Args: %s",
        identity, gameUuid, args);

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
                assert(getMessageHandler);
                auto& game_ = game.first->second;
                getMessageHandler->trySetDelegate(
                    uuid_for_game,
                    std::make_shared<GetMessageHandler>(
                        game_.getInfo(), nodePlayerControl));
                availableGames.emplace(uuid_for_game, &game_);
                return success(uuid_for_game);
            }
        }
    }
    return failure();
}

Reply<Uuid> BridgeMain::Impl::join(
    const Identity& identity, const std::optional<Uuid>& gameUuid,
    const std::optional<Uuid>& playerUuid, std::optional<Position> position)
{
    log(LogLevel::DEBUG, "Join command from %s. Game: %s. Player: %s. Position: %s",
        identity, gameUuid, playerUuid, position);

    const auto iter = nodes.find(identity);
    if (iter != nodes.end()) {
        if (iter->second == Role::PEER && (!playerUuid || !gameUuid)) {
            return failure();
        }
        auto uuid_for_game = Uuid {};
        auto game = static_cast<BridgeGame*>(nullptr);
        if (gameUuid || position || availableGames.empty()) {
            uuid_for_game = gameUuid.value_or(Uuid {});
            game = internalGetGame(uuid_for_game);
            if (game) {
                position = game->getPositionForPlayerToJoin(identity, position);
            }
        } else {
            while (!availableGames.empty()) {
                const auto& possible_game = availableGames.front();
                assert(possible_game.second);
                const auto possible_position =
                    possible_game.second->getPositionForPlayerToJoin(
                        identity, std::nullopt);
                if (possible_position) {
                    uuid_for_game = possible_game.first;
                    game = possible_game.second;
                    position = possible_position;
                    break;
                }
                availableGames.pop();
            }
        }
        if (game && position) {
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
    const Identity& identity, const Uuid& gameUuid,
    const std::optional<Uuid>& playerUuid, const Call& call)
{
    log(LogLevel::DEBUG, "Call command from %s. Game: %s. Player: %s. Call: %s",
        identity, gameUuid, playerUuid, call);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        const auto game = internalGetGame(gameUuid);
        if (game && game->call(identity, *player, call)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::play(
    const Identity& identity, const Uuid& gameUuid,
    const std::optional<Uuid>& playerUuid,
    const std::optional<CardType>& card,
    const std::optional<int>& index)
{
    log(LogLevel::DEBUG, "Play command from %s. Game: %s. Player: %s. Card: %s. Index: %d",
        identity, gameUuid, playerUuid, card, index);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        const auto game = internalGetGame(gameUuid);
        if (game && game->play(identity, *player, card, index)) {
            return success();
        }
    }
    return failure();
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
    const Identity& identity, const std::optional<Uuid>& playerUuid)
{
    assert(nodePlayerControl);
    return nodePlayerControl->getPlayer(identity, playerUuid).get();
}

BridgeMain::BridgeMain(Messaging::MessageContext& context, Config config) :
    impl {std::make_unique<Impl>(context, std::move(config))}
{
}

BridgeMain::~BridgeMain() = default;

void BridgeMain::run()
{
    assert(impl);
    impl->run();
}

}
}
