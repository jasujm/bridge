#include "main/BridgeMain.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/Call.hh"
#include "bridge/Card.hh"
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
#include "messaging/CommandUtility.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Logging.hh"
#include "Observer.hh"

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

using Engine::BridgeEngine;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::MessageQueue;
using Messaging::Reply;
using Messaging::success;

namespace {

using StringVector = std::vector<std::string>;
using VersionVector = std::vector<int>;
using PositionSet = std::set<Position>;

const std::string PEER_ROLE {"peer"};
const std::string CLIENT_ROLE {"client"};

auto generatePositionsControlled(const BridgeMain::PositionVector& positions)
{
    if (positions.empty()) {
        return PositionSet(POSITIONS.begin(), POSITIONS.end());
    }
    return PositionSet(positions.begin(), positions.end());
}

}

class BridgeMain::Impl :
    public Observer<BridgeEngine::ShufflingCompleted>,
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::BiddingCompleted>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::TrickCompleted>,
    public Observer<BridgeEngine::DummyRevealed>,
    public Observer<BridgeEngine::DealEnded> {
public:

    Impl(
        zmq::context_t& context, const std::string& baseEndpoint,
        PositionVector positions, const EndpointVector& peerEndpoints,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint);

    void run();

    void terminate();

    BridgeEngine& getEngine();

    void startIfReady();

private:

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfClient(
        const std::string& identity, const std::string& command,
        Args&&... args);

    void handleNotify(const BridgeEngine::ShufflingCompleted&) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::BiddingCompleted&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::TrickCompleted&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;

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

    boost::optional<Position> internalGetPositionForPlayer(
        boost::optional<Position> preferredPosition) const;
    const Player* internalGetPlayerFor(
        const std::string& identity, const boost::optional<Uuid>& playerUuid);
    void internalInitializePeers(
        zmq::context_t& context, const EndpointVector& peerEndpoints,
        const PositionVector& positions,
        const std::string& cardServerBasePeerEndpoint);
    bool internalArePositionsFree(const PositionVector& positions) const;

    bool peerMode;
    std::shared_ptr<NodePlayerControl> nodePlayerControl;
    std::shared_ptr<zmq::socket_t> eventSocket;
    BridgeGame bridgeGame;
    Messaging::MessageLoop messageLoop;
};

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& baseEndpoint,
    PositionVector positions, const EndpointVector& peerEndpoints,
    // TODO: Restore support for card server protocol
    const std::string& /*cardServerControlEndpoint*/,
    const std::string& /*cardServerBasePeerEndpoint*/) :
    peerMode {!peerEndpoints.empty()},
    nodePlayerControl(std::make_shared<NodePlayerControl>()),
    eventSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pub)},
    bridgeGame {generatePositionsControlled(positions), eventSocket}
{
    auto endpointIterator = Messaging::EndpointIterator {baseEndpoint};
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    controlSocket->bind(*endpointIterator++);
    eventSocket->bind(*endpointIterator);
    auto handlers = MessageQueue::HandlerMap {
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
            GET_COMMAND,
            std::make_shared<GetMessageHandler>(
                bridgeGame.gameManager, bridgeGame.engine, nodePlayerControl)
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
    };
    for (auto&& handler : bridgeGame.cardProtocol->getMessageHandlers()) {
        handlers.emplace(handler);
    }
    messageLoop.addSocket(
        std::move(controlSocket), MessageQueue { std::move(handlers) });
    for (auto&& socket : bridgeGame.cardProtocol->getSockets()) {
        messageLoop.addSocket(socket.first, socket.second);
    }
    if (peerMode) {
        // TODO: Restore card server protocol support
        internalInitializePeers(
            context, peerEndpoints, positions, "" /*cardServerBasePeerEndpoint*/);
    }
}


void BridgeMain::Impl::run()
{
    messageLoop.run();
}

void BridgeMain::Impl::terminate()
{
    messageLoop.terminate();
}

void BridgeMain::Impl::startIfReady()
{
    if (bridgeGame.positionsInUse.size() == N_POSITIONS) {
        assert(bridgeGame.engine);
        log(LogLevel::DEBUG, "Starting bridge engine");
        bridgeGame.cardProtocol->initialize();
        bridgeGame.engine->initiate();
    }
}

BridgeEngine& BridgeMain::Impl::getEngine()
{
    assert(bridgeGame.engine);
    return *bridgeGame.engine;
}

template<typename... Args>
void BridgeMain::Impl::publish(const std::string& command, Args&&... args)
{
    sendCommand(
        dereference(eventSocket), JsonSerializer {}, command,
        std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeers(
    const std::string& command, Args&&... args)
{
    assert(bridgeGame.peerCommandSender);
    bridgeGame.peerCommandSender->sendCommand(
        JsonSerializer {}, command, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeersIfClient(
    const std::string& identity, const std::string& command, Args&&... args)
{
    if (bridgeGame.clients.find(identity) != bridgeGame.clients.end()) {
        sendToPeers(command, std::forward<Args>(args)...);
    }
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::ShufflingCompleted&)
{
    log(LogLevel::DEBUG, "Shuffling completed");
    publish(DEAL_COMMAND);
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CallMade& event)
{
    assert(bridgeGame.engine);
    const auto position = bridgeGame.engine->getPosition(event.player);
    log(LogLevel::DEBUG, "Call made. Position: %s. Call: %s", position, event.call);
    publish(
        CALL_COMMAND,
        std::tie(POSITION_COMMAND, position),
        std::tie(CALL_COMMAND, event.call));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::BiddingCompleted&)
{
    log(LogLevel::DEBUG, "Bidding completed");
    publish(BIDDING_COMMAND);
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    assert(bridgeGame.engine);
    const auto hand_position = dereference(bridgeGame.engine->getPosition(event.hand));
    const auto card_type = event.card.getType().get();
    log(LogLevel::DEBUG, "Card played. Position: %s. Card: %s", hand_position,
        card_type);
    publish(
        PLAY_COMMAND,
        std::tie(POSITION_COMMAND, hand_position),
        std::tie(CARD_COMMAND, card_type));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::TrickCompleted& event)
{
    const auto position = dereference(bridgeGame.engine->getPosition(event.winner));
    log(LogLevel::DEBUG, "Trick completed. Winner: %s", position);
    publish(TRICK_COMMAND, std::tie(WINNER_COMMAND, position));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DummyRevealed&)
{
    log(LogLevel::DEBUG, "Dummy hand revealed");
    publish(DUMMY_COMMAND);
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    log(LogLevel::DEBUG, "Deal ended");
    publish(DEAL_END_COMMAND);
}

Reply<> BridgeMain::Impl::hello(
    const std::string& identity, const VersionVector& version,
    const std::string& role)
{
    log(LogLevel::DEBUG, "Hello command from %s. Version: %d. Role: %s",
        asHex(identity), version.empty() ? -1 : version.front(), role);
    if (version.empty() || version.front() > 0) {
        return failure();
    } else if (role == PEER_ROLE && peerMode) {
        log(LogLevel::DEBUG, "Peer accepted: %s", asHex(identity));
        bridgeGame.peers.emplace(identity, PositionVector {});
        return success();
    } else if (role == CLIENT_ROLE) {
        log(LogLevel::DEBUG, "Client accepted: %s", asHex(identity));
        bridgeGame.clients.insert(identity);
        return success();
    }
    return failure();
}

Reply<> BridgeMain::Impl::game(
    const std::string& identity, PositionVector positions,
    const boost::optional<nlohmann::json>& args)
{
    log(LogLevel::DEBUG, "Game command from %s", asHex(identity));

    if (bridgeGame.peers.find(identity) != bridgeGame.peers.end() &&
        internalArePositionsFree(positions)) {
        bridgeGame.peers[identity] = positions;
        for (const auto position : positions) {
            bridgeGame.positionsInUse.insert(position);
        }
        assert(bridgeGame.cardProtocol);
        if (bridgeGame.cardProtocol->acceptPeer(identity, positions, args)) {
            startIfReady();
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

    const auto iter = bridgeGame.peers.find(identity);
    if (iter != bridgeGame.peers.end()) {
        if (!playerUuid) {
            return failure();
        }
        const auto& controlled_positions = iter->second;
        const auto peer_controls_position = position && std::find(
            controlled_positions.begin(), controlled_positions.end(), *position)
            != controlled_positions.end();
        if (!peer_controls_position) {
            return failure();
        }
    }

    assert(bridgeGame.engine);
    position = internalGetPositionForPlayer(position);
    if (!position) {
        return failure();
    }

    assert(nodePlayerControl);
    auto player = nodePlayerControl->createPlayer(identity, playerUuid);
    sendToPeersIfClient(
        identity, JOIN_COMMAND,
        std::make_pair(std::cref(PLAYER_COMMAND), player->getUuid()),
        std::tie(POSITION_COMMAND, position.get()));

    assert(position);
    bridgeGame.engine->setPlayer(*position, std::move(player));

    return success();
}

Reply<> BridgeMain::Impl::call(
    const std::string& identity, const boost::optional<Uuid>& playerUuid,
    const Call& call)
{
    log(LogLevel::DEBUG, "Call command from %s. Player: %s. Call: %s",
        asHex(identity), playerUuid, call);
    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        assert(bridgeGame.engine);
        if (bridgeGame.engine->call(*player, call)) {
            sendToPeersIfClient(
                identity,
                CALL_COMMAND,
                std::make_pair(std::cref(PLAYER_COMMAND), player->getUuid()),
                std::tie(CALL_COMMAND, call));
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

    // Either card or index - but not both - needs to be provided
    if (bool(card) == bool(index)) {
        return failure();
    }

    if (const auto player = internalGetPlayerFor(identity, playerUuid)) {
        assert(bridgeGame.engine);
        if (const auto hand = bridgeGame.engine->getHandInTurn()) {
            const auto n_card = index ? index : findFromHand(*hand, *card);
            if (n_card) {
                if (bridgeGame.engine->play(*player, *hand, *n_card)) {
                    const auto player_position = bridgeGame.engine->getPosition(*player);
                    sendToPeersIfClient(
                        identity,
                        PLAY_COMMAND,
                        std::make_pair(std::cref(PLAYER_COMMAND), player->getUuid()),
                        std::tie(INDEX_COMMAND, *n_card));
                    return success();
                }
            }
        }
    }
    return failure();
}

void BridgeMain::Impl::internalInitializePeers(
    zmq::context_t& context, const EndpointVector& peerEndpoints,
    const PositionVector& positions,
    const std::string& cardServerBasePeerEndpoint)
{
    for (const auto& endpoint : peerEndpoints) {
        messageLoop.addSocket(
            bridgeGame.peerCommandSender->addPeer(context, endpoint),
            [&sender = *bridgeGame.peerCommandSender](zmq::socket_t& socket)
            {
                sender.processReply(socket);
                return true;
            });
    }
    sendToPeers(
        HELLO_COMMAND,
        std::make_pair(std::cref(VERSION_COMMAND), VersionVector {0}),
        std::tie(ROLE_COMMAND, PEER_ROLE));
    sendToPeers(
        GAME_COMMAND,
        std::tie(POSITIONS_COMMAND, positions),
        std::make_pair(
            std::cref(ARGS_COMMAND),
            makePeerArgsForCardServerProxy(cardServerBasePeerEndpoint)));
}

boost::optional<Position> BridgeMain::Impl::internalGetPositionForPlayer(
    boost::optional<Position> preferredPosition) const
{
    assert(bridgeGame.engine);
    if (preferredPosition) {
        return bridgeGame.engine->getPlayer(*preferredPosition) ?
            boost::none : preferredPosition;
    }
    for (const auto position : bridgeGame.positionsControlled) {
        if (!bridgeGame.engine->getPlayer(position)) {
            return position;
        }
    }
    return boost::none;
}

const Player* BridgeMain::Impl::internalGetPlayerFor(
    const std::string& identity, const boost::optional<Uuid>& playerUuid)
{
    assert(nodePlayerControl);
    return nodePlayerControl->getPlayer(identity, playerUuid).get();
}

bool BridgeMain::Impl::internalArePositionsFree(
    const PositionVector& positions) const
{
    return std::all_of(
        positions.begin(), positions.end(),
        [this](const auto p)
        {
            return bridgeGame.positionsInUse.find(p) == bridgeGame.positionsInUse.end();
        });
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
    auto& engine = impl->getEngine();
    engine.subscribeToShufflingCompleted(impl);
    engine.subscribeToCallMade(impl);
    engine.subscribeToBiddingCompleted(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToTrickCompleted(impl);
    engine.subscribeToDummyRevealed(impl);
    engine.subscribeToDealEnded(impl);
    impl->startIfReady();
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
