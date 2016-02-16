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
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/MessageQueue.hh"
#include "scoring/DuplicateScoreSheet.hh"

#include <zmq.hpp>

#include <array>
#include <iterator>
#include <thread>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};
const std::string BridgeMain::STATE_PREFIX {"state"};

using Messaging::makeMessageHandler;

class BridgeMain::Impl {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        const std::string& dataEndpoint);

    ~Impl();

private:

    std::tuple<> state();
    std::tuple<> call(const Call& call);
    std::tuple<> play(const CardType& card);
    std::tuple<Scoring::DuplicateScoreSheet> score();

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
                makeMessageHandler(
                    *this, &Impl::state, Messaging::JsonSerializer {})
            },
            {
                CALL_COMMAND,
                makeMessageHandler(
                    *this, &Impl::call, Messaging::JsonSerializer {})
            },
            {
                PLAY_COMMAND,
                makeMessageHandler(
                    *this, &Impl::play, Messaging::JsonSerializer {})
            },
            {
                SCORE_COMMAND,
                makeMessageHandler(
                    *this, &Impl::score, Messaging::JsonSerializer {})
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

std::tuple<> BridgeMain::Impl::state()
{
    using namespace Messaging;
    const auto messages = {
        JsonSerializer::serialize(makeDealState(engine))};
    sendMessage(dataSocket, STATE_PREFIX, true);
    sendMessage(dataSocket, messages.begin(), messages.end());
    return std::make_tuple();
}

std::tuple<> BridgeMain::Impl::call(const Call& call)
{
    if (const auto player = engine.getPlayerInTurn()) {
        engine.call(*player, call);
    }
    return state();
}

std::tuple<> BridgeMain::Impl::play(const CardType& card)
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

std::tuple<Scoring::DuplicateScoreSheet> BridgeMain::Impl::score()
{
    assert(gameManager);
    return std::make_tuple(gameManager->getScoreSheet());
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
