#include "main/BridgeGame.hh"

#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/CardProtocol.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "Utility.hh"

namespace Bridge {
namespace Main {

BridgeGame::BridgeGame(
    PositionSet positionsControlled,
    std::shared_ptr<zmq::socket_t> eventSocket) :
    gameManager {std::make_shared<Engine::DuplicateGameManager>()},
    positionsControlled {positionsControlled},
    positionsInUse {std::move(positionsControlled)},
    peerCommandSender {std::make_shared<PeerCommandSender>()},
    cardProtocol {std::make_unique<SimpleCardProtocol>(peerCommandSender)},
    engine {
        std::make_shared<Engine::BridgeEngine>(
            dereference(this->cardProtocol).getCardManager(), gameManager)},
    eventSocket {std::move(eventSocket)}
{
}

}
}
