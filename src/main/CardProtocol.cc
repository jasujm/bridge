#include "main/CardProtocol.hh"

namespace Bridge {
namespace Main {

CardProtocol::~CardProtocol() = default;

bool CardProtocol::acceptPeer(
    const std::string& identity, const PositionVector& positions,
    const OptionalArgs& args)
{
    return handleAcceptPeer(identity, positions, args);
}

void CardProtocol::initialize()
{
    handleInitialize();
}

CardProtocol::MessageHandlerVector CardProtocol::getMessageHandlers()
{
    return handleGetMessageHandlers();
}

CardProtocol::SocketVector CardProtocol::getSockets()
{
    return handleGetSockets();
}

std::shared_ptr<Engine::CardManager> CardProtocol::getCardManager()
{
    return handleGetCardManager();
}

}
}
