#include "main/PeerlessCardProtocol.hh"

#include "bridge/CardShuffle.hh"
#include "bridge/CardType.hh"

namespace Bridge {
namespace Main {

void PeerlessCardProtocol::internalShuffle(
    Engine::SimpleCardManager& cardManager,
    const Engine::CardManager::ShufflingState state)
{
    if (state == Engine::CardManager::ShufflingState::REQUESTED) {
        const auto cards = generateShuffledDeck();
        cardManager.shuffle(cards.begin(), cards.end());
    }
}

bool PeerlessCardProtocol::handleAcceptPeer(
    const Messaging::Identity&, const PositionVector&, const OptionalArgs&)
{
    return false;
}

void PeerlessCardProtocol::handleInitialize()
{
}

std::shared_ptr<Messaging::MessageHandler>
PeerlessCardProtocol::handleGetDealMessageHandler()
{
    return nullptr;
}

CardProtocol::SocketVector PeerlessCardProtocol::handleGetSockets()
{
    return {};
}

std::shared_ptr<Engine::CardManager>
PeerlessCardProtocol::handleGetCardManager()
{
    return cardManager;
}

}
}
