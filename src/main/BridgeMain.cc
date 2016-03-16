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
#include <stdexcept>

namespace Bridge {
namespace Main {

const std::string BridgeMain::HELLO_COMMAND {"bridgehlo"};
const std::string BridgeMain::STATE_COMMAND {"state"};
const std::string BridgeMain::CALL_COMMAND {"call"};
const std::string BridgeMain::PLAY_COMMAND {"play"};
const std::string BridgeMain::SCORE_COMMAND {"score"};

using Engine::BridgeEngine;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Scoring::DuplicateScoreSheet;

namespace {

struct PlayerEntry {
    PlayerEntry(
        const Player& player, zmq::context_t& context,
        const std::string& endpoint) :
        player {player},
        dataSocket {context, zmq::socket_type::pub}
    {
        dataSocket.bind(endpoint);
    }

    const Player& player;
    zmq::socket_t dataSocket;
};

}

class BridgeMain::Impl : public Observer<BridgeEngine::DealEnded> {
public:

    Impl(
        zmq::context_t& context, const std::string& controlEndpoint,
        DataEndpointList dataEndpoints);

    void run();

    void terminate();

    BridgeEngine& getEngine();

private:

    template<typename T>
    void publish(zmq::socket_t& socket, const std::string& command, const T& t);
    void publishState();

    void handleNotify(const BridgeEngine::DealEnded&);

    std::string hello(const std::string& indentity);
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
    zmq::context_t& context;
    DataEndpointList dataEndpoints;
    std::map<std::string, PlayerEntry> entries;
};

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& controlEndpoint,
    DataEndpointList dataEndpoints) :
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
    context {context},
    dataEndpoints {std::move(dataEndpoints)}
{
    if (this->dataEndpoints.size() < N_PLAYERS) {
        throw std::domain_error("At least four data endpoints needed");
    }
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
void BridgeMain::Impl::publish(
    zmq::socket_t& socket, const std::string& command, const T& t)
{
    sendCommand(socket, JsonSerializer {}, command, t);
}

void BridgeMain::Impl::publishState()
{
    for (auto& entry : entries) {
        publish(
            entry.second.dataSocket, STATE_COMMAND,
            makeDealState(engine, entry.second.player));
    }
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    assert(gameManager);
    const auto& score_sheet = gameManager->getScoreSheet();
    for (auto& entry : entries) {
        publish(
            entry.second.dataSocket, SCORE_COMMAND, score_sheet);
    }
}

std::string BridgeMain::Impl::hello(const std::string& identity)
{
    const auto n = entries.size();
    if (n >= N_PLAYERS) {
        return {};
    }

    assert(players.size() >= n);
    assert(players[n]);
    assert(!dataEndpoints.empty());

    entries.emplace(
        identity, PlayerEntry {*players[n], context, dataEndpoints.front()});
    auto ret = std::move(dataEndpoints.front());
    dataEndpoints.pop_front();
    return ret;
}

DealState BridgeMain::Impl::state(const std::string& identity)
{
    const auto iter = entries.find(identity);
    if (iter == entries.end()) {
        // TODO: Signal failure
        return DealState {};
    }

    return makeDealState(engine, iter->second.player);
}

bool BridgeMain::Impl::call(const std::string& identity, const Call& call)
{
    const auto iter = entries.find(identity);
    if (iter == entries.end()) {
        return false;
    }

    engine.call(iter->second.player, call);
    publishState();
    return true;
}

bool BridgeMain::Impl::play(const std::string& identity, const CardType& card)
{
    const auto iter = entries.find(identity);
    if (iter == entries.end()) {
        return false;
    }

    if (const auto hand = engine.getHandInTurn()) {
        if (const auto n_card = findFromHand(*hand, card)) {
            engine.play(iter->second.player, *hand, *n_card);
        }
    }
    publishState();
    return true;
}

DuplicateScoreSheet BridgeMain::Impl::score(const std::string& identity)
{
    const auto iter = entries.find(identity);
    if (iter == entries.end()) {
        // TODO: Signal failure
        return DuplicateScoreSheet {};
    }

    assert(gameManager);
    return gameManager->getScoreSheet();
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& controlEndpoint,
    DataEndpointList dataEndpoints) :
    impl {
        std::make_shared<Impl>(
            context, controlEndpoint, std::move(dataEndpoints))}
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
