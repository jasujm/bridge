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
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "scoring/DuplicateScoreSheet.hh"
#include "FunctionObserver.hh"
#include "Observer.hh"

#include <boost/iterator/indirect_iterator.hpp>
#include <zmq.hpp>

#include <array>
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

using AllowedCalls = std::vector<Call>;
using AllowedCards = std::vector<CardType>;

// TODO: This function is used to generate shuffled cards for
// SimpleCardManager whenever shuffling of cards is requested. In the future
// the cards need to be synchronized between peers.
auto makeCardShuffler(SimpleCardManager& cardManager)
{
    auto shuffler = makeObserver<CardManager::ShufflingState>(
        [&cardManager](const auto state)
        {
            if (state == CardManager::ShufflingState::REQUESTED) {
                std::random_device rd;
                std::default_random_engine re {rd()};
                auto cards = std::vector<CardType>(
                    cardTypeIterator(0),
                    cardTypeIterator(N_CARDS));
                std::shuffle(cards.begin(), cards.end(), re);
                cardManager.shuffle(cards.begin(), cards.end());
            }
        });
    cardManager.subscribe(shuffler);
    return shuffler;
}

template<typename PairIterator>
auto secondIterator(PairIterator iter)
{
    return boost::make_transform_iterator(
        iter, [](auto&& pair) -> decltype(auto) { return pair.second; });
}

}

const std::string BridgeMain::HELLO_COMMAND {"bridgehlo"};
const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};

class BridgeMain::Impl : public Observer<BridgeEngine::DealEnded> {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& eventEndpoint, PositionRange positions);

    void run();

    void terminate();

    BridgeEngine& getEngine();

    template<typename PositionIterator>
    auto positionPlayerIterator(PositionIterator iter);

private:

    template<typename... Args>
    void publish(const std::string& command, const Args&... args);

    void handleNotify(const BridgeEngine::DealEnded&) override;

    Reply<Position> hello(const std::string& indentity);
    Reply<DealState, AllowedCalls, AllowedCards> state(
        const std::string& identity, Position position);
    Reply<> call(
        const std::string& identity, Position position, const Call& call);
    Reply<> play(
        const std::string& identity, Position position, const CardType& card);
    Reply<DuplicateScoreSheet> score(const std::string& identity);

    std::shared_ptr<Engine::SimpleCardManager> cardManager {
        std::make_shared<Engine::SimpleCardManager>()};
    std::shared_ptr<Observer<CardManager::ShufflingState>> cardShuffler {
        makeCardShuffler(*cardManager)};
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
    Messaging::MessageQueue messageQueue;
    zmq::socket_t eventSocket;
    PeerClientControl peerClientControl;
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
    const std::string& eventEndpoint, PositionRange positions) :
    messageQueue {
        {
            {
                HELLO_COMMAND,
                makeMessageHandler(*this, &Impl::hello, JsonSerializer {})
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
        },
    context, controlEndpoint},
    eventSocket {context, zmq::socket_type::pub},
    peerClientControl {
        context,
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end())}
{
    eventSocket.bind(eventEndpoint);
}

void BridgeMain::Impl::run()
{
    messageQueue.run();
}

void BridgeMain::Impl::terminate()
{
    messageQueue.terminate();
}

BridgeEngine& BridgeMain::Impl::getEngine()
{
    return engine;
}

template<typename... Args>
void BridgeMain::Impl::publish(const std::string& command, const Args&... args)
{
    sendCommand(eventSocket, JsonSerializer {}, command, args...);
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    assert(gameManager);
    publish(SCORE_COMMAND, gameManager->getScoreSheet());
}

Reply<Position> BridgeMain::Impl::hello(const std::string& identity)
{
    if (const auto player = peerClientControl.addClient(identity)) {
        return success(engine.getPosition(*player));
    }
    return failure();
}

Reply<DealState, AllowedCalls, AllowedCards> BridgeMain::Impl::state(
    const std::string& identity, const Position position)
{
    const auto& player = engine.getPlayer(position);
    if (!peerClientControl.isAllowedToAct(identity, player)) {
        return failure();
    }

    const auto& deal_state = makeDealState(engine, player);
    auto allowed_calls = AllowedCalls {};
    auto allowed_cards = AllowedCards {};
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
        engine.call(player, call);
    }
    publish(CALL_COMMAND);
    peerClientControl.sendCommandIfSelfControlledPlayer(
        player, CALL_COMMAND, position, call);
    return success();
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
                engine.play(player, *hand, *n_card);
            }
        }
    }
    publish(PLAY_COMMAND);
    peerClientControl.sendCommandIfSelfControlledPlayer(
        player, PLAY_COMMAND, position, card);
    return success();
}

Reply<DuplicateScoreSheet> BridgeMain::Impl::score(const std::string&)
{
    assert(gameManager);
    return success(gameManager->getScoreSheet());
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& eventEndpoint, PositionRange positions) :
    impl {
        std::make_shared<Impl>(
            context, controlEndpoint, eventEndpoint, positions)}
{
    impl->getEngine().subscribe(impl);
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
