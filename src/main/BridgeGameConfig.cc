#include "main/BridgeGameConfig.hh"

#include "main/Commands.hh"
#include "main/BridgeGame.hh"
#include "main/CardServerProxy.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Logging.hh"

#include <boost/uuid/uuid_io.hpp>
#include <json.hpp>

#include <string>
#include <tuple>

namespace Bridge {
namespace Main {

namespace {

using namespace std::string_literals;

const auto VERSION_VECTOR = std::vector {0};
const auto PEER_ROLE = "peer"s;

}

bool operator==(
    const BridgeGameConfig::PeerConfig& lhs,
    const BridgeGameConfig::PeerConfig& rhs)
{
    return std::tie(lhs.endpoint, lhs.serverKey) ==
        std::tie(rhs.endpoint, rhs.serverKey);
}

bool operator==(
    const BridgeGameConfig::CardServerConfig& lhs,
    const BridgeGameConfig::CardServerConfig& rhs)
{
    return std::tie(lhs.controlEndpoint, lhs.basePeerEndpoint) ==
        std::tie(rhs.controlEndpoint, rhs.basePeerEndpoint);
}

BridgeGame gameFromConfig(
    const BridgeGameConfig& config,
    zmq::context_t& context,
    const Messaging::CurveKeys* keys,
    std::shared_ptr<zmq::socket_t> eventSocket,
    std::shared_ptr<CallbackScheduler> callbackScheduler)
{
    if (config.peers.empty()) {
        log(LogLevel::INFO, "Configuring game %s, no peers", config.uuid);
        return {
            config.uuid, std::move(eventSocket), std::move(callbackScheduler)};
    } else {
        log(LogLevel::INFO,
            "Configuring game %s, %s protocol", config.uuid,
            config.cardServer ? "card server" : "simple");
        auto peer_command_sender =
            std::make_shared<PeerCommandSender>(callbackScheduler);
        for (const auto& peer : config.peers) {
            peer_command_sender->addPeer(
                context, peer.endpoint, keys, peer.serverKey);
        }
        peer_command_sender->sendCommand(
            Messaging::JsonSerializer {},
            HELLO_COMMAND,
            std::make_tuple(
                std::cref(VERSION_COMMAND), std::cref(VERSION_VECTOR)),
            std::make_tuple(std::cref(ROLE_COMMAND), std::cref(PEER_ROLE)));
        auto game_args = nlohmann::json {
            { POSITIONS_COMMAND, config.positionsControlled },
        };
        if (config.cardServer) {
            auto card_server_args = nlohmann::json {
                { ENDPOINT_COMMAND, config.cardServer->basePeerEndpoint }
            };
            if (!config.cardServer->serverKey.empty()) {
                card_server_args[SERVER_KEY_COMMAND] =
                    config.cardServer->serverKey;
            }
            game_args[CARD_SERVER_COMMAND] = card_server_args;
        }
        peer_command_sender->sendCommand(
            Messaging::JsonSerializer {},
            GAME_COMMAND,
            std::make_pair(std::cref(GAME_COMMAND), std::cref(config.uuid)),
            std::make_pair(std::cref(ARGS_COMMAND), std::cref(game_args)));
        auto card_protocol = std::unique_ptr<CardProtocol> {};
        if (config.cardServer) {
            card_protocol = std::make_unique<CardServerProxy>(
                context, keys, config.cardServer->controlEndpoint);
        } else {
            card_protocol = std::make_unique<SimpleCardProtocol>(
                config.uuid, peer_command_sender);
        }
        auto positions = BridgeGame::PositionSet(
            config.positionsControlled.begin(),
            config.positionsControlled.end());
        return {
            config.uuid, std::move(positions), std::move(eventSocket),
            std::move(card_protocol), std::move(peer_command_sender),
            std::move(callbackScheduler)};
    }
}

}
}
