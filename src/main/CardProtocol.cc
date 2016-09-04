#include "main/CardProtocol.hh"

#include "bridge/Position.hh"

#include <utility>

namespace Bridge {
namespace Main {

CardProtocol::~CardProtocol() = default;

void CardProtocol::setAcceptor(std::weak_ptr<PeerAcceptor> acceptor)
{
    handleSetAcceptor(std::move(acceptor));
}

CardProtocol::MessageHandlerRange CardProtocol::getMessageHandlers()
{
    return handleGetMessageHandlers();
}

CardProtocol::SocketRange CardProtocol::getSockets()
{
    return handleGetSockets();
}

std::shared_ptr<Engine::CardManager> CardProtocol::getCardManager()
{
    return handleGetCardManager();
}

CardProtocol::PeerAcceptor::~PeerAcceptor() = default;

}
}
