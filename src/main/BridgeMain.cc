#include "main/BridgeMain.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "engine/BridgeEngine.hh"
#include "engine/MakeDealState.hh"
#include "main/PeerClientControl.hh"
#include "main/PeerCommandSender.hh"
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

#include <random>
#include <utility>

namespace Bridge {
namespace Main {

using Engine::BridgeEngine;
using Engine::CardManager;
using Engine::SimpleCardManager;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
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
    SimpleCardManager& getCardManager();

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

    void handleNotify(const BridgeEngine::DealEnded&) override;
    void handleNotify(const CardManager::ShufflingState& state) override;

    Reply<Position> hello(const std::string& indentity);
    Reply<> peer(const std::string& identity, const PositionVector& positions);
    Reply<DealState, CallVector, CardVector> state(
        const std::string& identity, Position position);
    Reply<> deal(const std::string& identity, const CardVector& cards);
    Reply<> call(
        const std::string& identity, Position position, const Call& call);
    Reply<> play(
        const std::string& identity, Position position, const CardType& card);
    Reply<DuplicateScoreSheet> score(const std::string& identity);

    std::shared_ptr<Engine::SimpleCardManager> cardManager {
        std::make_shared<Engine::SimpleCardManager>()};
    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::map<Position, std::shared_ptr<BasicPlayer>> players {
        {Position::NORTH, std::make_shared<BasicPlayer>()},
        {Position::EAST, std::make_shared<BasicPlayer>()},
        {Position::SOUTH, std::make_shared<BasicPlayer>()},
        {Position::WEST, std::make_shared<BasicPlayer>()}};
    BridgeEngine engine {
        cardManager, gameManager,
        secondIterator(players.begin()),
        secondIterator(players.end())};
    PeerCommandSender peerCommandSender;
    PeerClientControl peerClientControl;
    zmq::socket_t controlSocket;
    zmq::socket_t eventSocket;
    Messaging::MessageLoop messageLoop;
    const Player& leader {*players[Position::NORTH]};
    bool expectingCards {false};
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
    controlSocket {context, zmq::socket_type::router},
    eventSocket {context, zmq::socket_type::pub}
{
    controlSocket.bind(controlEndpoint);
    messageLoop.addSocket(
        controlSocket,
        Messaging::MessageQueue {
            {
                {
                    HELLO_COMMAND,
                    makeMessageHandler(*this, &Impl::hello, JsonSerializer {})
                },
                {
                    PEER_COMMAND,
                    makeMessageHandler(*this, &Impl::peer, JsonSerializer {})
                },
                {
                    DEAL_COMMAND,
                    makeMessageHandler(*this, &Impl::deal, JsonSerializer {})
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
            }
        });
    eventSocket.bind(eventEndpoint);
    for (const auto& endpoint : peerEndpoints) {
        auto socket = peerCommandSender.addPeer(context, endpoint);
        messageLoop.addSocket(
            *socket,
            [&sender = this->peerCommandSender](zmq::socket_t& socket)
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

SimpleCardManager& BridgeMain::Impl::getCardManager()
{
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
    peerCommandSender.sendCommand(JsonSerializer {}, command, args...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeersIfSelfControlledPlayer(
    const Player& player, const std::string& command, const Args&... args)
{
    if (peerClientControl.isSelfControlledPlayer(player)) {
        sendToPeers(command, args...);
    }
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    assert(gameManager);
    publish(SCORE_COMMAND, gameManager->getScoreSheet());
}

void BridgeMain::Impl::handleNotify(const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::REQUESTED) {
        if (peerClientControl.isSelfControlledPlayer(leader)) {
            std::random_device rd;
            std::default_random_engine re {rd()};
            auto cards = CardVector(
                cardTypeIterator(0),
                cardTypeIterator(N_CARDS));
            std::shuffle(cards.begin(), cards.end(), re);
            assert(cardManager);
            cardManager->shuffle(cards.begin(), cards.end());
            publish(DEAL_COMMAND);
            sendToPeers(DEAL_COMMAND, cards);
        } else {
            expectingCards = true;
        }
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

Reply<> BridgeMain::Impl::deal(
    const std::string& identity, const CardVector& cards)
{
    if (expectingCards &&
        peerClientControl.isAllowedToAct(identity, leader)) {
        cardManager->shuffle(cards.begin(), cards.end());
        publish(DEAL_COMMAND);
        return success();
    }
    return failure();
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
            publish(CALL_COMMAND);
            sendToPeersIfSelfControlledPlayer(
                player, CALL_COMMAND, position, call);
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
                    publish(PLAY_COMMAND);
                    sendToPeersIfSelfControlledPlayer(
                        player, PLAY_COMMAND, position, card);
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
    engine.subscribe(impl);
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
