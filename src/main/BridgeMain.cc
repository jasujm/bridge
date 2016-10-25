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
#include "main/PeerClientControl.hh"
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

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <zmq.hpp>

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

template<typename PairIterator>
auto secondIterator(PairIterator iter)
{
    return boost::make_transform_iterator(
        iter, [](auto&& pair) -> decltype(auto) { return pair.second; });
}

}

class BridgeMain::Impl :
    public CardProtocol::PeerAcceptor,
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::BiddingCompleted>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::TrickCompleted>,
    public Observer<BridgeEngine::DummyRevealed>,
    public Observer<BridgeEngine::DealEnded>,
    public Observer<CardManager::ShufflingState> {
public:

    Impl(
        zmq::context_t& context, const std::string& baseEndpoint,
        const PositionVector& positions, const EndpointVector& peerEndpoints,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint);

    void run();

    void terminate();

    CardProtocol& getCardProtocol();
    BridgeEngine& getEngine();

    template<typename PositionIterator>
    auto positionPlayerIterator(PositionIterator iter);

    std::unique_ptr<CardProtocol> makeCardProtocol(
        zmq::context_t& context,
        const std::string& cardServerControlEndpoint,
        const std::string& cardServerBasePeerEndpoint);

    bool readyToStart() const;
    void startIfReady();

private:

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfSelfControlledPlayer(
        const Player& player, const std::string& command, Args&&... args);

    CardProtocol::PeerAcceptState acceptPeer(
        const std::string& identity,
        const CardProtocol::PositionVector& positions) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::BiddingCompleted&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::TrickCompleted&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;
    void handleNotify(const CardManager::ShufflingState& state) override;

    const Player* getPlayerFor(
        const std::string& identity, const boost::optional<Position>& position);

    Reply<Position> hello(const std::string& indentity);
    Reply<> call(
        const std::string& identity, const boost::optional<Position>& position,
        const Call& call);
    Reply<> play(
        const std::string& identity, const boost::optional<Position>& position,
        const boost::optional<CardType>& card,
        const boost::optional<std::size_t>& index);

    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::map<Position, std::shared_ptr<BasicPlayer>> players {
        {Position::NORTH, std::make_shared<BasicPlayer>()},
        {Position::EAST, std::make_shared<BasicPlayer>()},
        {Position::SOUTH, std::make_shared<BasicPlayer>()},
        {Position::WEST, std::make_shared<BasicPlayer>()}};
    std::shared_ptr<PeerClientControl> peerClientControl;
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>()};
    std::unique_ptr<CardProtocol> cardProtocol;
    std::shared_ptr<BridgeEngine> engine;
    zmq::socket_t eventSocket;
    Messaging::MessageLoop messageLoop;
};

template<typename PositionIterator>
auto BridgeMain::Impl::positionPlayerIterator(PositionIterator iter)
{
    return boost::make_transform_iterator(
        iter,
        [&players = this->players](const auto position) -> const Player&
        {
            const auto& player = players.at(position);
            assert(player);
            return *player;
        });
}

std::unique_ptr<CardProtocol> BridgeMain::Impl::makeCardProtocol(
    zmq::context_t& context,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint)
{
    if (!cardServerControlEndpoint.empty() &&
        !cardServerBasePeerEndpoint.empty()) {
        log(LogLevel::DEBUG, "Card exchange protocol: card server");
        return std::make_unique<CardServerProxy>(
            context, cardServerControlEndpoint);
    }
    log(LogLevel::DEBUG, "Card exchange protocol: simple");
    return std::make_unique<SimpleCardProtocol>(peerCommandSender);
}

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& baseEndpoint,
    const PositionVector& positions, const EndpointVector& peerEndpoints,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint) :
    peerClientControl {
        std::make_shared<PeerClientControl>(
            positionPlayerIterator(positions.begin()),
            positionPlayerIterator(positions.end()))},
    cardProtocol {
        makeCardProtocol(
            context, cardServerControlEndpoint, cardServerBasePeerEndpoint)},
    engine {
        std::make_shared<BridgeEngine>(
            cardProtocol->getCardManager(), gameManager,
            secondIterator(players.begin()),
            secondIterator(players.end()))},
    eventSocket {context, zmq::socket_type::pub}
{
    auto endpointIterator = Messaging::EndpointIterator {baseEndpoint};
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->bind(*endpointIterator++);
    eventSocket.bind(*endpointIterator);
    auto handlers = MessageQueue::HandlerMap {
        {
            HELLO_COMMAND,
            makeMessageHandler(
                *this, &Impl::hello, JsonSerializer {},
                std::make_tuple(),
                std::make_tuple(POSITION_COMMAND))
        },
        {
            GET_COMMAND,
            std::make_shared<GetMessageHandler>(
                gameManager, engine, peerClientControl)
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
    sendToPeers(
        PEER_COMMAND,
        std::tie(POSITIONS_COMMAND, positions),
        std::tie(CARD_SERVER_COMMAND, cardServerBasePeerEndpoint));
}

void BridgeMain::Impl::run()
{
    messageLoop.run();
}

void BridgeMain::Impl::terminate()
{
    messageLoop.terminate();
}

bool BridgeMain::Impl::readyToStart() const
{
    assert(peerClientControl);
    return peerClientControl->arePlayersControlled(
        boost::make_indirect_iterator(secondIterator(players.begin())),
        boost::make_indirect_iterator(secondIterator(players.end())));
}

void BridgeMain::Impl::startIfReady()
{
    if (readyToStart()) {
        assert(engine);
        log(LogLevel::DEBUG, "Starting bridge engine");
        engine->initiate();
    }
}

CardProtocol& BridgeMain::Impl::getCardProtocol()
{
    assert(cardProtocol);
    return *cardProtocol;
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
    assert(peerClientControl);
    if (peerClientControl->isSelfControlledPlayer(player)) {
        sendToPeers(command, std::forward<Args>(args)...);
    }
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

void BridgeMain::Impl::handleNotify(const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::COMPLETED) {
        log(LogLevel::DEBUG, "Shuffling completed");
        publish(DEAL_COMMAND);
    }
}

const Player* BridgeMain::Impl::getPlayerFor(
    const std::string& identity, const boost::optional<Position>& position)
{
    assert(peerClientControl);
    if (position) {
        assert(engine);
        const auto& player = engine->getPlayer(*position);
        if (peerClientControl->isAllowedToAct(identity, player)) {
            return &player;
        }
        return nullptr;
    }
    return peerClientControl->getPlayer(identity);
}

Reply<Position> BridgeMain::Impl::hello(const std::string& identity)
{
    assert(peerClientControl);
    if (const auto player = peerClientControl->addClient(identity)) {
        assert(engine);
        const auto position = engine->getPosition(*player);
        log(LogLevel::DEBUG, "Client accepted: %s. Position: %s",
            asHex(identity), position);
        return success(position);
    }
    log(LogLevel::DEBUG, "Client rejected: %s", asHex(identity));
    return failure();
}

CardProtocol::PeerAcceptState BridgeMain::Impl::acceptPeer(
    const std::string& identity, const CardProtocol::PositionVector& positions)
{
    assert(peerClientControl);
    const auto success = peerClientControl->addPeer(
        identity,
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end()));
    if (success) {
        log(LogLevel::DEBUG, "Peer accepted: %s", asHex(identity));
        if (readyToStart()) {
            // Engine initialization is deferred to give the card protocol
            // opportunity to initialize before cards are requested
            assert(engine);
            messageLoop.callOnce(
                [&engine = *this->engine]() { engine.initiate(); });
            return CardProtocol::PeerAcceptState::ALL_ACCEPTED;
        }
        return CardProtocol::PeerAcceptState::ACCEPTED;
    }
    log(LogLevel::DEBUG, "Peer rejected: %s", identity);
    return CardProtocol::PeerAcceptState::REJECTED;
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

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& baseEndpoint,
    const PositionVector& positions, const EndpointVector& peerEndpoints,
    const std::string& cardServerControlEndpoint,
    const std::string& cardServerBasePeerEndpoint) :
    impl {
        std::make_shared<Impl>(
            context, baseEndpoint, std::move(positions),
            std::move(peerEndpoints), cardServerControlEndpoint,
            cardServerBasePeerEndpoint)}
{
    auto& card_protocol = impl->getCardProtocol();
    card_protocol.setAcceptor(impl);
    auto& engine = impl->getEngine();
    engine.subscribeToCallMade(impl);
    engine.subscribeToBiddingCompleted(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToTrickCompleted(impl);
    engine.subscribeToDummyRevealed(impl);
    engine.subscribeToDealEnded(impl);
    dereference(card_protocol.getCardManager()).subscribe(impl);
    impl->startIfReady();
}

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
