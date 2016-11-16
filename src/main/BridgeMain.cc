#include "main/BridgeMain.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/Call.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Hand.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "main/CardServerProxy.hh"
#include "main/Commands.hh"
#include "main/GetMessageHandler.hh"
#include "main/NodeControl.hh"
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
#include "Logging.hh"
#include "Observer.hh"
#include "Zip.hh"

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/function_input_iterator.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <zmq.hpp>

#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

using Engine::BridgeEngine;
using Engine::CardManager;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::MessageQueue;
using Messaging::Reply;
using Messaging::success;

namespace {

using StringVector = std::vector<std::string>;
using VersionVector = std::vector<int>;
using PlayerVector = std::vector<std::shared_ptr<Player>>;

auto makePlayers(BridgeMain::PositionVector& positions)
{
    std::sort(positions.begin(), positions.end());
    const auto last = std::unique(positions.begin(), positions.end());
    if (last != positions.end()) {
        log(LogLevel::DEBUG, "Removing duplicates from position vector");
        positions.erase(last, positions.end());
    }
    struct Generator {
        using result_type = PlayerVector::value_type;
        auto operator()() const { return std::make_shared<BasicPlayer>(); }
    } generator;
    return PlayerVector(
        boost::make_function_input_iterator(generator, std::size_t {0u}),
        boost::make_function_input_iterator(generator, positions.size()));
}

const std::string PEER_ROLE {"peer"};
const std::string CLIENT_ROLE {"client"};

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

    std::unique_ptr<CardProtocol> makeCardProtocol(
        zmq::context_t& context,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint);

    void startIfReady();

private:

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfSelfControlledPlayer(
        const Player& player, const std::string& command, Args&&... args);

    void handleNotify(const BridgeEngine::ShufflingCompleted&) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::BiddingCompleted&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::TrickCompleted&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;

    const Player* getPlayerFor(
        const std::string& identity, const boost::optional<Position>& position);

    Reply<> hello(
        const std::string& identity, const VersionVector& version,
        const std::string& role);
    Reply<> game(
        const std::string& identity, boost::optional<PositionVector> positions);
    Reply<> call(
        const std::string& identity, const boost::optional<Position>& position,
        const Call& call);
    Reply<> play(
        const std::string& identity, const boost::optional<Position>& position,
        const boost::optional<CardType>& card,
        const boost::optional<std::size_t>& index);

    void internalAddPlayers(
        const PositionVector& positions, const PlayerVector& players);
    bool internalArePositionsFree(const PositionVector& positions) const;
    bool internalAddClientToPosition(
        const std::string& identity, const PositionVector& positions);

    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    PlayerVector players;
    std::set<std::string> peers;
    std::set<std::string> clients;
    std::shared_ptr<NodeControl> nodeControl;
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>()};
    std::unique_ptr<CardProtocol> cardProtocol;
    std::shared_ptr<BridgeEngine> engine;
    zmq::socket_t eventSocket;
    Messaging::MessageLoop messageLoop;
};

std::unique_ptr<CardProtocol> BridgeMain::Impl::makeCardProtocol(
    zmq::context_t& /*context*/,
    const std::string& /*cardServerControlEndpoint*/,
    const std::string& /*cardServerBasePeerEndpoint*/)
{
    // TODO: Use card server proxy if applicable
    log(LogLevel::DEBUG, "Card exchange protocol: simple");
    return std::make_unique<SimpleCardProtocol>(peerCommandSender);
}

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& baseEndpoint,
    PositionVector positions, const EndpointVector& peerEndpoints,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint) :
    players {makePlayers(positions)},
    nodeControl {
        std::make_shared<NodeControl>(
            boost::make_indirect_iterator(players.begin()),
            boost::make_indirect_iterator(players.end()))},
    cardProtocol {
        makeCardProtocol(
            context, cardServerControlEndpoint, cardServerBasePeerEndpoint)},
    engine {
        std::make_shared<BridgeEngine>(
            cardProtocol->getCardManager(), gameManager)},
    eventSocket {context, zmq::socket_type::pub}
{
    internalAddPlayers(positions, players);
    auto endpointIterator = Messaging::EndpointIterator {baseEndpoint};
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->setsockopt(ZMQ_ROUTER_HANDOVER, 1);
    controlSocket->bind(*endpointIterator++);
    eventSocket.bind(*endpointIterator);
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
                std::make_tuple(POSITIONS_COMMAND))
        },
        {
            GET_COMMAND,
            std::make_shared<GetMessageHandler>(
                gameManager, engine, nodeControl)
        },
        {
            CALL_COMMAND,
            makeMessageHandler(
                *this, &Impl::call, JsonSerializer {},
                std::make_tuple(POSITION_COMMAND, CALL_COMMAND))
        },
        {
            PLAY_COMMAND,
            makeMessageHandler(
                *this, &Impl::play, JsonSerializer {},
                std::make_tuple(POSITION_COMMAND, CARD_COMMAND, INDEX_COMMAND))
        }
    };
    for (auto&& handler : cardProtocol->getMessageHandlers()) {
        handlers.emplace(handler);
    }
    messageLoop.addSocket(
        std::move(controlSocket), MessageQueue { std::move(handlers) });
    for (const auto& endpoint : peerEndpoints) {
        messageLoop.addSocket(
            peerCommandSender->addPeer(context, endpoint),
            [&sender = *this->peerCommandSender](zmq::socket_t& socket)
            {
                sender.processReply(socket);
                return true;
            });
    }
    for (auto&& socket : cardProtocol->getSockets()) {
        messageLoop.addSocket(socket.first, socket.second);
    }
    const auto version = VersionVector {0};
    sendToPeers(
        HELLO_COMMAND,
        std::tie(VERSION_COMMAND, version), std::tie(ROLE_COMMAND, PEER_ROLE));
    sendToPeers(GAME_COMMAND, std::tie(POSITIONS_COMMAND, positions));
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
    if (players.size() == N_PLAYERS) {
        assert(engine);
        log(LogLevel::DEBUG, "Starting bridge engine");
        cardProtocol->initialize();
        engine->initiate();
    }
}

BridgeEngine& BridgeMain::Impl::getEngine()
{
    assert(engine);
    return *engine;
}

template<typename... Args>
void BridgeMain::Impl::publish(const std::string& command, Args&&... args)
{
    sendCommand(
        eventSocket, JsonSerializer {}, command, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeers(
    const std::string& command, Args&&... args)
{
    assert(peerCommandSender);
    peerCommandSender->sendCommand(
        JsonSerializer {}, command, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeersIfSelfControlledPlayer(
    const Player& player, const std::string& command, Args&&... args)
{
    assert(nodeControl);
    if (nodeControl->isSelfRepresentedPlayer(player)) {
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
    assert(engine);
    const auto position = engine->getPosition(event.player);
    log(LogLevel::DEBUG, "Call made. Position: %s. Call: %s", position, event.call);
    publish(
        CALL_COMMAND,
        std::tie(POSITION_COMMAND, position),
        std::tie(CALL_COMMAND, event.call));
    sendToPeersIfSelfControlledPlayer(
        event.player,
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
    assert(engine);
    const auto hand_position = dereference(engine->getPosition(event.hand));
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
    const auto position = dereference(engine->getPosition(event.winner));
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

const Player* BridgeMain::Impl::getPlayerFor(
    const std::string& identity, const boost::optional<Position>& position)
{
    assert(nodeControl);
    if (position) {
        assert(engine);
        const auto player = engine->getPlayer(*position);
        if (player && nodeControl->isAllowedToAct(identity, *player)) {
            return player;
        }
        return nullptr;
    }
    return nodeControl->getPlayer(identity);
}

Reply<> BridgeMain::Impl::hello(
    const std::string& identity, const VersionVector& version,
    const std::string& role)
{
    log(LogLevel::DEBUG, "Hello command from %s. Version: %d. Role: %s",
        asHex(identity), version.empty() ? -1 : version.front(), role);
    if (version.empty() || version.front() > 0) {
        return failure();
    } else if (role == PEER_ROLE) {
        log(LogLevel::DEBUG, "Peer accepted: %s", asHex(identity));
        peers.insert(identity);
        return success();
    } else if (role == CLIENT_ROLE) {
        log(LogLevel::DEBUG, "Client accepted: %s", asHex(identity));
        clients.insert(identity);
        return success();
    }
    return failure();
}

Reply<> BridgeMain::Impl::game(
    const std::string& identity, boost::optional<PositionVector> positions)
{
    log(LogLevel::DEBUG, "Game command from %s", asHex(identity));
    assert(nodeControl);

    if (peers.find(identity) != peers.end() && positions &&
        internalArePositionsFree(*positions)) {
        auto new_players = makePlayers(*positions);
        const auto success_ = nodeControl->addPeer(
            identity,
            boost::make_indirect_iterator(new_players.begin()),
            boost::make_indirect_iterator(new_players.end()));
        if (success_) {
            assert(cardProtocol);
            if (cardProtocol->acceptPeer(identity, *positions)) {
                internalAddPlayers(*positions, new_players);
                std::move(
                    new_players.begin(), new_players.end(),
                    std::back_inserter(players));
                startIfReady();
                return success();
            }
        }
    } else if (clients.find(identity) != clients.end()) {
        if (!positions) {
            positions.emplace(POSITIONS.begin(), POSITIONS.end());
        }
        if (internalAddClientToPosition(identity, *positions)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::call(
    const std::string& identity, const boost::optional<Position>& position,
    const Call& call)
{
    log(LogLevel::DEBUG, "Call command from %s. Position: %s. Call: %s",
        asHex(identity), position, call);
    if (const auto player = getPlayerFor(identity, position)) {
        assert(engine);
        if (engine->call(*player, call)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::play(
    const std::string& identity, const boost::optional<Position>& position,
    const boost::optional<CardType>& card,
    const boost::optional<std::size_t>& index)
{
    log(LogLevel::DEBUG, "Play command from %s. Position: %s. Card: %s. Index: %d",
        asHex(identity), position, card, index);

    // Either card or index - but not both - needs to be provided
    if (bool(card) == bool(index)) {
        return failure();
    }

    if (const auto player = getPlayerFor(identity, position)) {
        assert(engine);
        if (const auto hand = engine->getHandInTurn()) {
            const auto n_card = index ? index : findFromHand(*hand, *card);
            if (n_card) {
                if (engine->play(*player, *hand, *n_card)) {
                    const auto player_position = engine->getPosition(*player);
                    sendToPeersIfSelfControlledPlayer(
                        *player,
                        PLAY_COMMAND,
                        std::tie(POSITION_COMMAND, player_position),
                        std::tie(INDEX_COMMAND, *n_card));
                    return success();
                }
            }
        }
    }
    return failure();
}

void BridgeMain::Impl::internalAddPlayers(
    const PositionVector& positions, const PlayerVector& players)
{
    assert(engine);
    for (const auto t : zip(positions, players)) {
        engine->setPlayer(t.get<0>(), t.get<1>());
    }

}

bool BridgeMain::Impl::internalArePositionsFree(
    const PositionVector& positions) const
{
    assert(engine);
    return std::none_of(
        positions.begin(), positions.end(),
        [&engine = *this->engine](const auto position)
        {
            return engine.getPlayer(position);
        });
}

bool BridgeMain::Impl::internalAddClientToPosition(
    const std::string& identity, const PositionVector& positions)
{
    for (const auto position : positions) {
        if (const auto player = engine->getPlayer(position)) {
            if (nodeControl->addClient(identity, *player)) {
                return true;
            }
        }
    }
    return false;
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
