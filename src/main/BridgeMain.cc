#include "main/BridgeMain.hh"

#include "bridge/GameState.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "bridge/Hand.hh"
#include "engine/MakeGameState.hh"
#include "Utility.hh"

#include <algorithm>

namespace Bridge {
namespace Main {

namespace {

void processQueue(BridgeMain::EventQueue& queue)
{
    while (!queue.empty()) {
        (queue.front())();
        queue.pop();
    }
}

void enqueueAndProcess(
    BridgeMain::EventQueue& queue,
    const BridgeMain::EventQueue::value_type event)
{
    queue.emplace(std::move(event));
    if (queue.size() == 1) { // if queue was empty
        processQueue(queue);
    }
}

}

BridgeMain::BridgeMain(
    std::unique_ptr<Engine::BridgeEngine> engine,
    std::vector<std::shared_ptr<Player>> players) :
    engine {std::move(engine)},
    players {std::move(players)}
{
}

void BridgeMain::init(Engine::CardManager& cardManager)
{
    cardManager.subscribe(shared_from_this());
    enqueueAndProcess(
        events,
        [&engine = dereference(engine)]()
        {
            engine.initiate();
        });
}

void BridgeMain::handleCall(const Call& call)
{
    if (const auto player = internalGetPlayer()) {
        enqueueAndProcess(
            events,
            [&engine = dereference(engine), &player = *player, &call]()
            {
                engine.process_event(Engine::CallEvent {player, call});
            });
    }
}

void BridgeMain::handlePlay(const CardType& cardType)
{
    if (const auto player = internalGetPlayer()) {
        if (const auto hand = engine->getHand(*player)) {
            if (const auto n_card = findFromHand(*hand, cardType)) {
                enqueueAndProcess(
                    events,
                    [&engine = dereference(engine),
                     &player = *player, n_card = *n_card]()
                    {
                        engine.process_event(
                            Engine::PlayCardEvent {player, n_card});
                    });
            }
        }
    }
}

GameState BridgeMain::handleGetState() const
{
    return makeGameState(dereference(engine));
}

void BridgeMain::handleNotify(const Engine::Shuffled&)
{
    enqueueAndProcess(
        events,
        [&engine = dereference(engine)]()
        {
            engine.process_event(Engine::ShuffledEvent {});
        });
}

Player* BridgeMain::internalGetPlayer()
{
    if (const auto player_in_turn = dereference(engine).getPlayerInTurn()) {
        const auto iter = std::find_if(
            players.begin(), players.end(),
            [player_in_turn](const auto& player_ptr)
            {
                return player_ptr.get() == player_in_turn;
            });
        if (iter != players.end()) {
            return iter->get();
        }
    }
    return nullptr;
}

}
}
