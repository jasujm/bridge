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

const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};

using Engine::BridgeEngine;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Scoring::DuplicateScoreSheet;

class BridgeMain::Impl : public Observer<BridgeEngine::DealEnded> {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& dataEndpoint);

    void run();

    void terminate();

    BridgeEngine& getEngine();

private:

    template<typename T>
    void publish(const std::string& command, const T& t);

    void publishState();

    void handleNotify(const BridgeEngine::DealEnded&);

    DealState state(const std::string& identity);
    bool call(const std::string& identity, const Call& call);
    bool play(const std::string& identity, const CardType& card);
    DuplicateScoreSheet score(const std::string& identity);

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
    zmq::socket_t dataSocket;
};

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& dataEndpoint) :
    messageQueue {
        {
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
    dataSocket {context, zmq::socket_type::pub}
{
    dataSocket.bind(dataEndpoint);
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

template<typename T>
void BridgeMain::Impl::publish(const std::string& command, const T& t)
{
    sendCommand(dataSocket, JsonSerializer {}, command, t);
}

void BridgeMain::Impl::publishState()
{
    publish(STATE_COMMAND, makeDealState(engine));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    assert(gameManager);
    publish(SCORE_COMMAND, gameManager->getScoreSheet());
}

DealState BridgeMain::Impl::state(const std::string&)
{
    return makeDealState(engine);
}

bool BridgeMain::Impl::call(const std::string&, const Call& call)
{
    if (const auto player = engine.getPlayerInTurn()) {
        engine.call(*player, call);
    }
    publishState();
    return true;
}

bool BridgeMain::Impl::play(const std::string&, const CardType& card)
{
    if (const auto player = engine.getPlayerInTurn()) {
        if (const auto hand = engine.getHand(*player)) {
            if (const auto n_card = findFromHand(*hand, card)) {
                engine.play(*player, *n_card);
            }
        }
    }
    publishState();
    return true;
}

DuplicateScoreSheet BridgeMain::Impl::score(const std::string&)
{
    assert(gameManager);
    return gameManager->getScoreSheet();
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& dataEndpoint) :
    impl {std::make_shared<Impl>(context, controlEndpoint, dataEndpoint)}
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
