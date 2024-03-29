#include "main/BridgeMain.hh"

#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "bridge/Deal.hh"
#include "bridge/Position.hh"
#include "bridge/UuidGenerator.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/BridgeGame.hh"
#include "main/BridgeGameConfig.hh"
#include "main/BridgeGameRecorder.hh"
#include "main/CardProtocol.hh"
#include "main/Commands.hh"
#include "main/Config.hh"
#include "main/GameStateHelper.hh"
#include "main/NodePlayerControl.hh"
#include "main/PeerCommandSender.hh"
#include "main/PeerlessCardProtocol.hh"
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
#include "messaging/Security.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Enumerate.hh"
#include "Logging.hh"

#include <boost/uuid/uuid_io.hpp>

#include <iterator>
#include <optional>
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
using Messaging::UserId;

namespace {

using GameMessageHandler = Messaging::DispatchingMessageHandler<
    Uuid, JsonSerializer>;

auto initializeGameMessageHandler()
{
    return Messaging::makeDispatchingMessageHandler<Uuid>(
        stringToBlob(GAME_COMMAND), JsonSerializer {});
}

auto initializeBridgeGameRecorder(std::optional<std::string_view> path)
{
    if (path) {
        return std::make_shared<BridgeGameRecorder>(*path);
    }
    return std::shared_ptr<BridgeGameRecorder> {};
}

bool isValidUuidArg(const Uuid& uuid)
{
    return !uuid.is_nil();
}

bool isValidUuidArg(const std::optional<Uuid>& uuid)
{
    return !uuid || isValidUuidArg(*uuid);
}

using namespace std::string_view_literals;
const auto VERSION = "0.1"sv;
const auto PEER_ROLE = "peer"sv;
const auto CLIENT_ROLE = "client"sv;

enum class Role {
    PEER,
    CLIENT
};

using namespace BlobLiterals;
const auto UNKNOWN_SUFFIX = ":UNK"_B;
const auto NOT_FOUND_SUFFIX = ":NF"_B;
const auto PEER_REJECTED_SUFFIX = ":PR"_B;
const auto ALREADY_EXISTS_SUFFIX = ":AE"_B;
const auto NOT_AUTHORIZED_SUFFIX = ":NA"_B;
const auto SEAT_RESERVED_SUFFIX = ":SR"_B;
const auto RULE_VIOLATION_SUFFIX = ":RV"_B;

}

class BridgeMain::Impl {
public:

    Impl(Messaging::MessageContext& context, Config config);

    void run();

private:

    Reply<> hello(
        const Identity& identity, const std::string& version,
        const std::string& role);
    Reply<Uuid> game(
        const Identity& identity, const std::optional<Uuid>& uuid,
        const std::optional<nlohmann::json>& args);
    Reply<Uuid, Position> join(
        const Identity& identity, const std::optional<Uuid>& gameUuid,
        const Uuid& playerUuid, std::optional<Position> positions);
    Reply<std::optional<Position>> leave(
        const Identity& identity, const Uuid& gameUuid, const Uuid& playerUuid);
    Reply<GameState, BridgeGame::Counter> get(
        const Identity& identity,
        const std::optional<Uuid>& gameUuid,
        const std::optional<Uuid>& dealUuid,
        const std::optional<Uuid>& playerUuid,
        const std::optional<std::vector<std::string>>& keys);
    Reply<> call(
        const Identity& identity, const Uuid& gameUuid,
        const Uuid& playerUuid, const Call& call);
    Reply<> play(
        const Identity& identity, const Uuid& gameUuid,
        const Uuid& playerUuid, const std::optional<CardType>& card,
        const std::optional<int>& index);

    BridgeGame* internalGetGame(const Uuid& gameUuid);
    BridgeGame* internalCreateGameFromRecord(
        const Uuid& gameUuid,
        BridgeGameRecorder::GameState&& gameState,
        BridgeGameRecorder::DealState&& dealState);
    std::shared_ptr<Player> internalGetOrCreatePlayer(
        const UserId& userId, const Uuid& playerUuid);
    std::unique_ptr<Deal> internalGetDeal(const Uuid& dealUuid);

    const Config config;
    std::map<Identity, Role> nodes;
    std::shared_ptr<NodePlayerControl> nodePlayerControl;
    Messaging::SharedSocket eventSocket;
    std::shared_ptr<GameMessageHandler> dealMessageHandler {
        initializeGameMessageHandler()};
    Messaging::MessageQueue messageQueue;
    Messaging::MessageLoop messageLoop;
    Messaging::Authenticator authenticator;
    std::shared_ptr<Messaging::PollingCallbackScheduler> callbackScheduler;
    std::map<Uuid, BridgeGame> games;
    std::optional<Uuid> defaultGameUuid;
    std::shared_ptr<BridgeGameRecorder> recorder;
    const bool recordPlayers;
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
                    std::tuple {VERSION_COMMAND, ROLE_COMMAND})
            },
            {
                stringToBlob(GAME_COMMAND),
                makeMessageHandler(
                    *this, &Impl::game, JsonSerializer {},
                    std::tuple {GAME_COMMAND, ARGS_COMMAND},
                    std::tuple {GAME_COMMAND})
            },
            {
                stringToBlob(JOIN_COMMAND),
                makeMessageHandler(
                    *this, &Impl::join, JsonSerializer {},
                    std::tuple {GAME_COMMAND, PLAYER_COMMAND, POSITION_COMMAND},
                    std::tuple {GAME_COMMAND, POSITION_COMMAND})
            },
            {
                stringToBlob(LEAVE_COMMAND),
                makeMessageHandler(
                    *this, &Impl::leave, JsonSerializer {},
                    std::tuple {GAME_COMMAND, PLAYER_COMMAND},
                    std::tuple {POSITION_COMMAND})
            },
            {
                stringToBlob(CALL_COMMAND),
                makeMessageHandler(
                    *this, &Impl::call, JsonSerializer {},
                    std::tuple {GAME_COMMAND, PLAYER_COMMAND, CALL_COMMAND})
            },
            {
                stringToBlob(PLAY_COMMAND),
                makeMessageHandler(
                    *this, &Impl::play, JsonSerializer {},
                    std::tuple {
                        GAME_COMMAND, PLAYER_COMMAND, CARD_COMMAND,
                        INDEX_COMMAND})
            },
            {
                stringToBlob(DEAL_COMMAND),
                dealMessageHandler
            },
            {
                stringToBlob(GET_COMMAND),
                makeMessageHandler(
                    *this, &Impl::get, JsonSerializer {},
                    std::tuple {GAME_COMMAND, DEAL_COMMAND, PLAYER_COMMAND, GET_COMMAND},
                    std::tuple {GET_COMMAND, COUNTER_COMMAND}),
            }
        }},
    messageLoop {context},
    authenticator {
        context, messageLoop.createTerminationSubscriber(),
        this->config.getKnownNodes()},
    callbackScheduler {
        std::make_shared<Messaging::PollingCallbackScheduler>(
            context, messageLoop.createTerminationSubscriber())},
    recorder {initializeBridgeGameRecorder(this->config.getDataDir())},
    recordPlayers {
        recorder != nullptr && this->config.getCurveConfig() != nullptr}
{
    authenticator.ensureRunning();
    const auto keys = this->config.getCurveConfig();
    auto endpointIterator = this->config.getEndpointIterator();
    auto controlSocket = Messaging::makeSharedSocket(
        context, Messaging::SocketType::router);
    controlSocket->set(zmq::sockopt::router_handover, true);
    Messaging::setupCurveServer(*controlSocket, keys);
    Messaging::bindSocket(*controlSocket, *endpointIterator++);
    Messaging::setupCurveServer(*eventSocket, keys);
    Messaging::bindSocket(*eventSocket, *endpointIterator++);
    for (auto& gameConfig : this->config.getGameConfigs()) {
        const auto& uuid = gameConfig.uuid;
        const auto [emplaced_game, game_created] = games.emplace(
            uuid,
            gameFromConfig(
                gameConfig, context, keys, eventSocket, callbackScheduler,
                this->config.getKnownNodes()));
        if (game_created) {
            auto& game = emplaced_game->second;
            if (gameConfig.isDefault) {
                if (defaultGameUuid) {
                    log(LogLevel::WARNING, "Multiple games defined as default. Ignoring %s",
                        gameConfig.uuid);
                } else {
                    defaultGameUuid = uuid;
                }
            }
            if (const auto peer_command_sender = game.getPeerCommandSender()) {
                for (auto&& [socket, cb] : peer_command_sender->getSockets()) {
                    messageLoop.addPollable(std::move(socket), std::move(cb));
                }
            }
            auto& card_protocol = game.getCardProtocol();
            if (auto&& hdl = card_protocol.getDealMessageHandler()) {
                dealMessageHandler->trySetDelegate(uuid, std::move(hdl));
            }
            for (auto&& [socket, cb] : card_protocol.getSockets()) {
                messageLoop.addPollable(std::move(socket), std::move(cb));
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
    const Identity& identity, const std::string& version,
    const std::string& role)
{
    log(LogLevel::DEBUG, "Hello command from %s. Version: %d. Role: %s",
        identity, version, role);
    if (version != "0.1"sv) {
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
    if (iter == nodes.end()) {
        return failure(UNKNOWN_SUFFIX);
    }

    if (!isValidUuidArg(gameUuid)) {
        return failure();
    }

    if (iter->second == Role::PEER) {
        if (!gameUuid || !args) {
            return failure();
        }
        if (const auto game = internalGetGame(*gameUuid)) {
            if (game->addPeer(identity, *args)) {
                return success(*gameUuid);
            } else {
                return failure(PEER_REJECTED_SUFFIX);
            }
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
    } else {
        const auto uuid_for_game = gameUuid ? *gameUuid : generateUuid();
        if (internalGetGame(uuid_for_game) == nullptr) {
            games.emplace(
                std::piecewise_construct,
                std::tuple {uuid_for_game},
                std::tuple {
                    uuid_for_game, eventSocket,
                    std::make_unique<PeerlessCardProtocol>(),
                    std::make_shared<Engine::DuplicateGameManager>(),
                    callbackScheduler, recorder});
            return success(uuid_for_game);
        } else {
            return failure(ALREADY_EXISTS_SUFFIX);
        }
    }
}

Reply<Uuid, Position> BridgeMain::Impl::join(
    const Identity& identity, const std::optional<Uuid>& gameUuid,
    const Uuid& playerUuid, std::optional<Position> position)
{
    log(LogLevel::DEBUG, "Join command from %s. Game: %s. Player: %s. Position: %s",
        identity, gameUuid, playerUuid, position);

    const auto iter = nodes.find(identity);
    if (iter == nodes.end()) {
        return failure(UNKNOWN_SUFFIX);
    }

    if (!isValidUuidArg(gameUuid) || !isValidUuidArg(playerUuid)) {
        return failure();
    }

    if (auto player = internalGetOrCreatePlayer(identity.userId, playerUuid)) {
        if (iter->second == Role::PEER && !gameUuid) {
            return failure();
        }
        auto uuid_for_game = Uuid {};
        auto game = static_cast<BridgeGame*>(nullptr);
        if (gameUuid) {
            uuid_for_game = *gameUuid;
        } else if (defaultGameUuid) {
            uuid_for_game = *defaultGameUuid;
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
        game = internalGetGame(uuid_for_game);
        if (game) {
            position = game->getPositionForPlayerToJoin(
                identity, position, *player);
            if (!position) {
                return failure(SEAT_RESERVED_SUFFIX);
            }
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
        assert(game);
        assert(position);
        if (game->join(identity, *position, std::move(player))) {
            return success(uuid_for_game, *position);
        } else {
            return failure(SEAT_RESERVED_SUFFIX);
        }
    } else {
        return failure(NOT_AUTHORIZED_SUFFIX);
    }
}

Reply<std::optional<Position>> BridgeMain::Impl::leave(
    const Identity& identity, const Uuid& gameUuid, const Uuid& playerUuid)
{
    log(LogLevel::DEBUG, "Leave command from %s. Game: %s. Player: %s",
        identity, gameUuid, playerUuid);

    const auto iter = nodes.find(identity);
    if (iter == nodes.end()) {
        return failure(UNKNOWN_SUFFIX);
    }

    if (!isValidUuidArg(gameUuid) || !isValidUuidArg(playerUuid)) {
        return failure();
    }

    if (auto player = internalGetOrCreatePlayer(identity.userId, playerUuid)) {
        if (const auto game = internalGetGame(gameUuid)) {
            const auto position = game->leave(identity, *player);
            return success(position);
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
    } else {
        return failure(NOT_AUTHORIZED_SUFFIX);
    }
}

Reply<GameState, BridgeGame::Counter> BridgeMain::Impl::get(
    const Identity& identity,
    const std::optional<Uuid>& gameUuid,
    const std::optional<Uuid>& dealUuid,
    const std::optional<Uuid>& playerUuid,
    const std::optional<std::vector<std::string>>& keys)
{
    log(LogLevel::DEBUG, "Get command from %s. Game: %s. Player: %s",
        identity, gameUuid, playerUuid);

    const auto node_iter = nodes.find(identity);
    if (node_iter == nodes.end()) {
        return failure(UNKNOWN_SUFFIX);
    }

    if (!isValidUuidArg(gameUuid) ||
        !isValidUuidArg(dealUuid) ||
        !isValidUuidArg(playerUuid) ||
        (bool(gameUuid) == bool(dealUuid))) {
        return failure();
    }

    if (node_iter->second == Role::PEER) {
        return failure();
    }

    auto player = std::shared_ptr<Player> {};
    if (playerUuid) {
        player = internalGetOrCreatePlayer(identity.userId, *playerUuid);
        if (!player) {
            return failure(NOT_AUTHORIZED_SUFFIX);
        }
    }

    if (gameUuid) {
        if (const auto game = internalGetGame(*gameUuid)) {
            return success(game->getState(player.get(), keys), game->getCounter());
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
    } else {
        assert(dealUuid);
        if (const auto deal = internalGetDeal(*dealUuid)) {
            auto deal_object = GameState {};
            emplacePubstate(deal.get(), deal_object);
            return success(std::move(deal_object), 0);
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
    }
}

Reply<> BridgeMain::Impl::call(
    const Identity& identity, const Uuid& gameUuid, const Uuid& playerUuid,
    const Call& call)
{
    log(LogLevel::DEBUG, "Call command from %s. Game: %s. Player: %s. Call: %s",
        identity, gameUuid, playerUuid, call);

    const auto iter = nodes.find(identity);
    if (iter == nodes.end()) {
        return failure(UNKNOWN_SUFFIX);
    }

    if (!isValidUuidArg(gameUuid) || !isValidUuidArg(playerUuid)) {
        return failure();
    }

    if (const auto player = internalGetOrCreatePlayer(identity.userId, playerUuid)) {
        if (const auto game = internalGetGame(gameUuid)) {
            if (game->call(identity, *player, call)) {
                return success();
            } else {
                return failure(RULE_VIOLATION_SUFFIX);
            }
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
    } else {
        return failure(NOT_AUTHORIZED_SUFFIX);
    }
}

Reply<> BridgeMain::Impl::play(
    const Identity& identity, const Uuid& gameUuid,
    const Uuid& playerUuid, const std::optional<CardType>& card,
    const std::optional<int>& index)
{
    log(LogLevel::DEBUG, "Play command from %s. Game: %s. Player: %s. Card: %s. Index: %d",
        identity, gameUuid, playerUuid, card, index);

    const auto iter = nodes.find(identity);
    if (iter == nodes.end()) {
        return failure(UNKNOWN_SUFFIX);
    }

    if (!isValidUuidArg(gameUuid) || !isValidUuidArg(playerUuid)) {
        return failure();
    }

    if (const auto player = internalGetOrCreatePlayer(identity.userId, playerUuid)) {
        if (const auto game = internalGetGame(gameUuid)) {
            if (game->play(identity, *player, card, index)) {
                return success();
            } else {
                return failure(RULE_VIOLATION_SUFFIX);
            }
        } else {
            return failure(NOT_FOUND_SUFFIX);
        }
    } else {
        return failure(NOT_AUTHORIZED_SUFFIX);
    }
}

BridgeGame* BridgeMain::Impl::internalGetGame(const Uuid& gameUuid)
{
    const auto iter = games.find(gameUuid);
    if (iter != games.end()) {
        return &iter->second;
    } else if (recorder) {
        if (auto game_state = recorder->recallGame(gameUuid)) {
            if (const auto& deal_uuid = game_state->dealUuid) {
                if (auto deal_state = recorder->recallDeal(*deal_uuid)) {
                    try {
                        return internalCreateGameFromRecord(
                            gameUuid, std::move(*game_state),
                            std::move(*deal_state));
                    } catch (const Engine::BridgeEngineFailure& e) {
                        log(LogLevel::WARNING,
                            "Error encountered when recalling deal %s",
                            *deal_uuid);
                    }
                }
            }
        }
    }
    return nullptr;
}

BridgeGame* BridgeMain::Impl::internalCreateGameFromRecord(
    const Uuid& gameUuid,
    BridgeGameRecorder::GameState&& gameState,
    BridgeGameRecorder::DealState&& dealState)
{
    auto& [deal, card_protocol, game_manager] = dealState;
    auto players = BridgeGame::PositionPlayerMap {};
    for (const auto [n, position] : enumerate(Position::all())) {
        if (const auto& player_uuid = gameState.playerUuids[n]) {
            const auto user_id = recordPlayers ?
                recorder->recallPlayer(*player_uuid) :
                std::optional<UserId> {};
            auto player = internalGetOrCreatePlayer(
                user_id ? *user_id : UserId {}, *player_uuid);
            players.emplace(position, std::move(player));
        }
    }
    auto&& game = games.emplace(
        std::piecewise_construct,
        std::tuple {gameUuid},
        std::tuple {
            gameUuid, eventSocket, std::move(card_protocol),
            std::move(game_manager), callbackScheduler, recorder,
            std::move(deal), std::move(players)});
    return &game.first->second;
}

std::shared_ptr<Player> BridgeMain::Impl::internalGetOrCreatePlayer(
    const UserId& userId, const Uuid& playerUuid)
{
    assert(nodePlayerControl);
    const auto recorder = recordPlayers ? this->recorder.get() : nullptr;
    return nodePlayerControl->getOrCreatePlayer(userId, playerUuid, recorder);
}

std::unique_ptr<Deal> BridgeMain::Impl::internalGetDeal(
    const Uuid& dealUuid)
{
    if (recorder) {
        if (auto deal_state = recorder->recallDeal(dealUuid)) {
            return std::move(deal_state->deal);
        }
    }
    return nullptr;
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
