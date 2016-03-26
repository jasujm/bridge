#include "main/BridgeMain.hh"

#include "bridge/BasicPlayer.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/DealState.hh"
#include "bridge/Hand.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "engine/BridgeEngine.hh"
#include "engine/MakeDealState.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/MessageQueue.hh"
#include "scoring/DuplicateScoreSheet.hh"
#include "Observer.hh"

#include <zmq.hpp>

#include <array>
#include <iterator>
#include <utility>

namespace Bridge {
namespace Main {

const std::string BridgeMain::HELLO_COMMAND {"bridgehlo"};
const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};

using Engine::BridgeEngine;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::Reply;
using Messaging::success;
using Scoring::DuplicateScoreSheet;

class BridgeMain::Impl : public Observer<BridgeEngine::DealEnded> {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& eventEndpoint);

    void run();

    void terminate();

    BridgeEngine& getEngine();

private:

    template<typename... Args>
    void publish(const std::string& command, const Args&... args);

    void handleNotify(const BridgeEngine::DealEnded&);

    Reply<> hello(const std::string& indentity);
    Reply<DealState> state(const std::string& identity);
    Reply<> call(const std::string& identity, const Call& call);
    Reply<> play(const std::string& identity, const CardType& card);
    Reply<DuplicateScoreSheet> score(const std::string& identity);

    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::array<std::shared_ptr<BasicPlayer>, N_PLAYERS> players {{
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>()}};
    BridgeEngine engine {
        std::make_shared<Engine::SimpleCardManager>(),
        gameManager, players.begin(), players.end()};
    Messaging::MessageQueue messageQueue;
    zmq::socket_t eventSocket;
    std::map<std::string, std::reference_wrapper<const Player>> clients;
};

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& eventEndpoint) :
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
    eventSocket {context, zmq::socket_type::pub}
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

Reply<> BridgeMain::Impl::hello(const std::string& identity)
{
    const auto n = clients.size();
    if (n >= N_PLAYERS) {
        return failure();
    }

    assert(players.size() >= n);
    assert(players[n]);
    clients.emplace(identity, *players[n]);
    return success();
}

Reply<DealState> BridgeMain::Impl::state(const std::string& identity)
{
    const auto iter = clients.find(identity);
    if (iter == clients.end()) {
        return failure();
    }

    return success(makeDealState(engine, iter->second));
}

Reply<> BridgeMain::Impl::call(const std::string& identity, const Call& call)
{
    const auto iter = clients.find(identity);
    if (iter == clients.end()) {
        return failure();
    }

    engine.call(iter->second, call);
    publish(CALL_COMMAND);
    return success();
}

Reply<> BridgeMain::Impl::play(const std::string& identity, const CardType& card)
{
    const auto iter = clients.find(identity);
    if (iter == clients.end()) {
        return failure();
    }

    if (const auto hand = engine.getHandInTurn()) {
        if (const auto n_card = findFromHand(*hand, card)) {
            engine.play(iter->second, *hand, *n_card);
        }
    }
    publish(PLAY_COMMAND);
    return success();
}

Reply<DuplicateScoreSheet> BridgeMain::Impl::score(const std::string& identity)
{
    const auto iter = clients.find(identity);
    if (iter == clients.end()) {
        return failure();
    }

    assert(gameManager);
    return success(gameManager->getScoreSheet());
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& eventEndpoint) :
    impl {
        std::make_shared<Impl>(context, controlEndpoint, eventEndpoint)}
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
