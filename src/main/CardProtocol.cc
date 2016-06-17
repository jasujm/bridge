#include "main/CardProtocol.hh"

namespace Bridge {
namespace Main {

CardProtocol::~CardProtocol() = default;

CardProtocol::MessageHandlerRange CardProtocol::getMessageHandlers()
{
    return handleGetMessageHandlers();
}

std::shared_ptr<Engine::CardManager> CardProtocol::getCardManager()
{
    return handleGetCardManager();
}

}
}
