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

#include <zmq.hpp>

#include <array>
#include <iterator>
#include <thread>
#include <utility>

namespace Bridge {
namespace Main {

const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};
const std::string BridgeMain::STATE_PREFIX {"state"};
const std::string BridgeMain::SCORE_PREFIX {"score"};

using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;

class BridgeMain::Impl {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& dataEndpoint);

    ~Impl();

private:

    bool state();
    bool call(const Call& call);
    bool play(const CardType& card);
    bool score();

    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::array<std::shared_ptr<BasicPlayer>, N_PLAYERS> players {{
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>()}};
    Engine::BridgeEngine engine {
        std::make_shared<Engine::SimpleCardManager>(),
        gameManager, players.begin(), players.end()};
    Messaging::MessageQueue messageQueue;
    zmq::socket_t dataSocket;
    std::thread thread;
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
    dataSocket {context, zmq::socket_type::pub},
    thread {[&messageQueue = this->messageQueue]() { messageQueue.run(); }}
{
    dataSocket.bind(dataEndpoint);
}

BridgeMain::Impl::~Impl()
{
    thread.join();
}

bool BridgeMain::Impl::state()
{
    sendCommand(
        dataSocket, JsonSerializer {}, STATE_PREFIX, makeDealState(engine));
    return true;
}

bool BridgeMain::Impl::call(const Call& call)
{
    if (const auto player = engine.getPlayerInTurn()) {
        engine.call(*player, call);
    }
    return state();
}

bool BridgeMain::Impl::play(const CardType& card)
{
    if (const auto player = engine.getPlayerInTurn()) {
        if (const auto hand = engine.getHand(*player)) {
            if (const auto n_card = findFromHand(*hand, card)) {
                engine.play(*player, *n_card);
            }
        }
    }
    return state();
}

bool BridgeMain::Impl::score()
{
    assert(gameManager);
    sendCommand(
        dataSocket, JsonSerializer {}, SCORE_PREFIX,
        gameManager->getScoreSheet());
    return true;
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& controlEndpoint,
    const std::string& dataEndpoint) :
    impl {std::make_unique<Impl>(context, controlEndpoint, dataEndpoint)}
{
}

BridgeMain::~BridgeMain() = default;

}
}
