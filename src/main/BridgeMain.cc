#include "main/BridgeMain.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/Call.hh"
#include "bridge/DealState.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "engine/MakeDealState.hh"
#include "main/PeerClientControl.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "scoring/DuplicateScoreSheet.hh"
#include "Observer.hh"

#include <boost/iterator/indirect_iterator.hpp>
#include <zmq.hpp>

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
using Scoring::DuplicateScoreSheet;

namespace {

using CallVector = std::vector<Call>;
using CardVector = std::vector<CardType>;
using PositionVector = std::vector<Position>;

template<typename PairIterator>
auto secondIterator(PairIterator iter)
{
    return boost::make_transform_iterator(
        iter, [](auto&& pair) -> decltype(auto) { return pair.second; });
}

}

const std::string BridgeMain::HELLO_COMMAND {"bridgehlo"};
const std::string BridgeMain::PEER_COMMAND {"bridgerp"};
const std::string BridgeMain::DEAL_COMMAND {"deal"};
const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};

class BridgeMain::Impl :
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::DealEnded>,
    public Observer<CardManager::ShufflingState> {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& eventEndpoint, PositionRange positions,
        EndpointRange peerEndpoints);

    void run();

    void terminate();

    BridgeEngine& getEngine();
    CardManager& getCardManager();

    template<typename PositionIterator>
    auto positionPlayerIterator(PositionIterator iter);

private:

    template<typename... Args>
    void publish(const std::string& command, const Args&... args);

    template<typename... Args>
    void sendToPeers(const std::string& command, const Args&... args);

    template<typename... Args>
    void sendToPeersIfSelfControlledPlayer(
        const Player& player, const std::string& command, const Args&... args);

    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;
    void handleNotify(const CardManager::ShufflingState& state) override;

    Reply<Position> hello(const std::string& indentity);
    Reply<> peer(const std::string& identity, const PositionVector& positions);
    Reply<DealState, CallVector, CardVector> state(
        const std::string& identity, Position position);
    Reply<> call(
        const std::string& identity, Position position, const Call& call);
    Reply<> play(
        const std::string& identity, Position position, const CardType& card);
    Reply<DuplicateScoreSheet> score(const std::string& identity);

    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::map<Position, std::shared_ptr<BasicPlayer>> players {
        {Position::NORTH, std::make_shared<BasicPlayer>()},
        {Position::EAST, std::make_shared<BasicPlayer>()},
        {Position::SOUTH, std::make_shared<BasicPlayer>()},
        {Position::WEST, std::make_shared<BasicPlayer>()}};
    PeerClientControl peerClientControl;
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>()};
    SimpleCardProtocol cardProtocol {
        [this](const auto identity)
        {
            const auto& leader = players.at(Position::NORTH);
            assert(leader);
            if (identity) {
                return peerClientControl.isAllowedToAct(*identity, *leader);
            }
            return peerClientControl.isSelfControlledPlayer(*leader);
        },
        peerCommandSender
    };
    BridgeEngine engine {
        cardProtocol.getCardManager(), gameManager,
        secondIterator(players.begin()),
        secondIterator(players.end())};
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

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& eventEndpoint, PositionRange positions,
    EndpointRange peerEndpoints) :
    peerClientControl {
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end())},
    eventSocket {context, zmq::socket_type::pub}
{
    eventSocket.bind(eventEndpoint);
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->bind(controlEndpoint);
    auto handlers = MessageQueue::HandlerMap {
        {
            HELLO_COMMAND,
            makeMessageHandler(*this, &Impl::hello, JsonSerializer {})
        },
        {
            PEER_COMMAND,
            makeMessageHandler(*this, &Impl::peer, JsonSerializer {})
        },
        {
            STATE_COMMAND,
            makeMessageHandler(*this, &Impl::state, JsonSerializer {})
        },
        {
            CALL_COMMAND,
            makeMessageHandler(*this, &Impl::call, JsonSerializer {})
        },
        {
            PLAY_COMMAND,
            makeMessageHandler(*this, &Impl::play, JsonSerializer {})
        },
        {
            SCORE_COMMAND,
            makeMessageHandler(*this, &Impl::score, JsonSerializer {})
        }
    };
    for (auto&& handler : cardProtocol.getMessageHandlers()) {
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
    sendToPeers(
        PEER_COMMAND, PositionVector(positions.begin(), positions.end()));
}

void BridgeMain::Impl::run()
{
    messageLoop.run();
}

void BridgeMain::Impl::terminate()
{
    messageLoop.terminate();
}

BridgeEngine& BridgeMain::Impl::getEngine()
{
    return engine;
}

CardManager& BridgeMain::Impl::getCardManager()
{
    const auto cardManager = cardProtocol.getCardManager();
    assert(cardManager);
    return *cardManager;
}

template<typename... Args>
void BridgeMain::Impl::publish(const std::string& command, const Args&... args)
{
    sendCommand(eventSocket, JsonSerializer {}, command, args...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeers(
    const std::string& command, const Args&... args)
{
    assert(peerCommandSender);
    peerCommandSender->sendCommand(JsonSerializer {}, command, args...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeersIfSelfControlledPlayer(
    const Player& player, const std::string& command, const Args&... args)
{
    if (peerClientControl.isSelfControlledPlayer(player)) {
        sendToPeers(command, args...);
    }
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CallMade& event)
{
    const auto position = engine.getPosition(event.player);
    publish(CALL_COMMAND);
    sendToPeersIfSelfControlledPlayer(
        event.player, CALL_COMMAND, position, event.call);
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    const auto position = engine.getPosition(event.player);
    publish(PLAY_COMMAND);
    sendToPeersIfSelfControlledPlayer(
        event.player, PLAY_COMMAND, position, event.card.getType().get());
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    assert(gameManager);
    publish(SCORE_COMMAND, gameManager->getScoreSheet());
}

void BridgeMain::Impl::handleNotify(const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::COMPLETED) {
        publish(DEAL_COMMAND);
    }
}

Reply<Position> BridgeMain::Impl::hello(const std::string& identity)
{
    if (const auto player = peerClientControl.addClient(identity)) {
        return success(engine.getPosition(*player));
    }
    return failure();
}

Reply<> BridgeMain::Impl::peer(
    const std::string& identity, const PositionVector& positions)
{
    const auto success_ = peerClientControl.addPeer(
        identity,
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end()));
    if (success_) {
        return success();
    }
    return failure();
}

Reply<DealState, CallVector, CardVector> BridgeMain::Impl::state(
    const std::string& identity, const Position position)
{
    const auto& player = engine.getPlayer(position);
    if (!peerClientControl.isAllowedToAct(identity, player)) {
        return failure();
    }

    const auto& deal_state = makeDealState(engine, player);
    auto allowed_calls = CallVector {};
    auto allowed_cards = CardVector {};
    if (engine.getPlayerInTurn() == &player) {
        if (const auto bidding = engine.getBidding()) {
            getAllowedCalls(*bidding, std::back_inserter(allowed_calls));
        }
        if (const auto trick = engine.getCurrentTrick()) {
            getAllowedCards(*trick, std::back_inserter(allowed_cards));
        }
    }
    return success(
        deal_state, std::move(allowed_calls), std::move(allowed_cards));
}

Reply<> BridgeMain::Impl::call(
    const std::string& identity, const Position position, const Call& call)
{
    const auto& player = engine.getPlayer(position);
    if (!peerClientControl.isAllowedToAct(identity, player)) {
        return failure();
    }

    if (peerClientControl.isAllowedToAct(identity, player)) {
        if (engine.call(player, call)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::play(
    const std::string& identity, const Position position, const CardType& card)
{
    const auto& player = engine.getPlayer(position);
    if (!peerClientControl.isAllowedToAct(identity, player)) {
        return failure();
    }

    if (peerClientControl.isAllowedToAct(identity, player)) {
        if (const auto hand = engine.getHandInTurn()) {
            if (const auto n_card = findFromHand(*hand, card)) {
                if (engine.play(player, *hand, *n_card)) {
                    return success();
                }
            }
        }
    }
    return failure();
}

Reply<DuplicateScoreSheet> BridgeMain::Impl::score(const std::string&)
{
    assert(gameManager);
    return success(gameManager->getScoreSheet());
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& eventEndpoint, PositionRange positions,
    EndpointRange peerEndpoints) :
    impl {
        std::make_shared<Impl>(
            context, controlEndpoint, eventEndpoint, positions, peerEndpoints)}
{
    auto& engine = impl->getEngine();
    engine.subscribeToCallMade(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToDealEnded(impl);
    impl->getCardManager().subscribe(impl);
    engine.initiate();
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
