#include "main/BridgeGameConfig.hh"

#include "bridge/Deal.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/Commands.hh"
#include "main/BridgeGame.hh"
#include "main/CardServerProxy.hh"
#include "main/PeerCommandSender.hh"
#include "main/PeerlessCardProtocol.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Logging.hh"

#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <tuple>

namespace Bridge {
namespace Main {

namespace {

using namespace std::string_view_literals;
const auto VERSION = "0.1"sv;
const auto PEER_ROLE = "peer"sv;

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
    return std::tie(lhs.controlEndpoint, lhs.peerEndpoint) ==
        std::tie(rhs.controlEndpoint, rhs.peerEndpoint);
}

BridgeGame gameFromConfig(
    const BridgeGameConfig& config,
    Messaging::MessageContext& context,
    const Messaging::CurveKeys* keys,
    Messaging::SharedSocket eventSocket,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    const Messaging::Authenticator::NodeMap& knownPeers)
{
    if (config.peers.empty()) {
        log(LogLevel::INFO, "Configuring game %s, no peers", config.uuid);
        return {
            config.uuid, std::move(eventSocket),
            std::make_unique<PeerlessCardProtocol>(),
            std::make_shared<Engine::DuplicateGameManager>(),
            std::move(callbackScheduler)};
    } else {
        log(LogLevel::INFO,
            "Configuring game %s, %s protocol", config.uuid,
            config.cardServer ? "card server" : "simple");
        auto peer_command_sender =
            std::make_shared<PeerCommandSender>(callbackScheduler);
        auto participants = BridgeGame::IdentitySet {};
        for (const auto& peer : config.peers) {
            peer_command_sender->addPeer(
                context, peer.endpoint, keys, peer.serverKey);
            if (!peer.serverKey.empty()) {
                participants.emplace(knownPeers.at(peer.serverKey));
            }
        }
        peer_command_sender->sendCommand(
            Messaging::JsonSerializer {},
            HELLO_COMMAND,
            std::pair {VERSION_COMMAND, VERSION},
            std::pair {ROLE_COMMAND, PEER_ROLE});
        auto game_args = nlohmann::json {
            { POSITIONS_COMMAND, config.positionsControlled },
        };
        if (config.cardServer) {
            auto card_server_args = nlohmann::json {
                { ENDPOINT_COMMAND, config.cardServer->peerEndpoint }
            };
            if (!config.cardServer->serverKey.empty()) {
                card_server_args.emplace(
                    SERVER_KEY_COMMAND, config.cardServer->serverKey);
            }
            game_args.emplace(CARD_SERVER_COMMAND, std::move(card_server_args));
        }
        peer_command_sender->sendCommand(
            Messaging::JsonSerializer {},
            GAME_COMMAND,
            std::pair {GAME_COMMAND, config.uuid},
            std::pair {ARGS_COMMAND, std::move(game_args)});
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
            std::move(card_protocol),
            std::make_shared<Engine::DuplicateGameManager>(),
            std::move(peer_command_sender),
            std::move(callbackScheduler),std::move(participants)};
    }
}

}
}
