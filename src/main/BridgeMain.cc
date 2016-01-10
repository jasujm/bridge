#include "main/BridgeMain.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/GameState.hh"
#include "engine/SimpleCardManager.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/CommandInterpreter.hh"
#include "main/SimpleBridgeController.hh"

#include <array>

namespace Bridge {
namespace Main {

class BridgeMain::Impl {
public:
    std::shared_ptr<Engine::SimpleCardManager> cardManager {
        std::make_shared<Engine::SimpleCardManager>()};
    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::array<std::shared_ptr<BasicPlayer>, N_PLAYERS> players {{
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>(),
        std::make_shared<BasicPlayer>()}};
    SimpleBridgeController controller {
        cardManager, gameManager, players.begin(), players.end()};
    CommandInterpreter interpreter {controller};
};

BridgeMain::BridgeMain() :
    impl {std::make_unique<Impl>()}
{
}

BridgeMain::~BridgeMain() = default;

void BridgeMain::processCommand(const std::string& command)
{
    assert(impl);
    impl->interpreter.interpret(command);
}

GameState BridgeMain::getState() const
{
    assert(impl);
    return impl->controller.getState();
}

const Engine::DuplicateGameManager& BridgeMain::getGameManager() const
{
    assert(impl);
    assert(impl->gameManager);
    return *impl->gameManager;
}

}
}
