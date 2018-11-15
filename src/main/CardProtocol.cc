#include "main/CardProtocol.hh"

namespace Bridge {
namespace Main {

CardProtocol::~CardProtocol() = default;

bool CardProtocol::acceptPeer(
    const Messaging::Identity& identity, const PositionVector& positions,
    const OptionalArgs& args)
{
    return handleAcceptPeer(identity, positions, args);
}

void CardProtocol::initialize()
{
    handleInitialize();
}

std::shared_ptr<Messaging::MessageHandler> CardProtocol::getDealMessageHandler()
{
    return handleGetDealMessageHandler();
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
