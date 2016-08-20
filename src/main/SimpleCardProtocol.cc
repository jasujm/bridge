#include "main/SimpleCardProtocol.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "engine/SimpleCardManager.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"

#include <array>
#include <random>
#include <string>
#include <utility>

namespace Bridge {
namespace Main {

namespace {

const auto CARDS_COMMAND = std::string {"cards"};

}

using CardVector = std::vector<CardType>;

using Engine::CardManager;
using Messaging::JsonSerializer;
using Messaging::Reply;

const std::string SimpleCardProtocol::DEAL_COMMAND {"deal"};

class SimpleCardProtocol::Impl :
    public Bridge::Observer<CardManager::ShufflingState> {
public:

    Impl(
        IsLeaderFunction isLeader,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

    Reply<> deal(const std::string& identity, const CardVector& cards);

    const std::shared_ptr<Engine::SimpleCardManager> cardManager {
        std::make_shared<Engine::SimpleCardManager>()};

    const std::array<MessageHandlerRange::value_type, 1> messageHandlers {{
        {
            DEAL_COMMAND,
            Messaging::makeMessageHandler(
                *this, &Impl::deal, JsonSerializer {}, CARDS_COMMAND)
        },
    }};

private:

    void handleNotify(const CardManager::ShufflingState& state) override;

    bool expectingCards {false};
    const IsLeaderFunction isLeader;
    const std::shared_ptr<PeerCommandSender> peerCommandSender;
};

SimpleCardProtocol::Impl::Impl(
    IsLeaderFunction isLeader,
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    isLeader {std::move(isLeader)},
    peerCommandSender {std::move(peerCommandSender)}
{
}

Reply<> SimpleCardProtocol::Impl::deal(
    const std::string& identity, const CardVector& cards)
{
    if (expectingCards && isLeader(&identity)) {
        cardManager->shuffle(cards.begin(), cards.end());
        expectingCards = false;
        return Messaging::success();
    }
    return Messaging::failure();
}

void SimpleCardProtocol::Impl::handleNotify(
    const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::REQUESTED) {
        if (isLeader(nullptr)) {
            std::random_device rd;
            std::default_random_engine re {rd()};
            auto cards = CardVector(
                cardTypeIterator(0),
                cardTypeIterator(N_CARDS));
            std::shuffle(cards.begin(), cards.end(), re);
            assert(cardManager);
            cardManager->shuffle(cards.begin(), cards.end());
            dereference(peerCommandSender).sendCommand(
                JsonSerializer {},
                DEAL_COMMAND,
                std::make_pair(CARDS_COMMAND, cards));
        } else {
            expectingCards = true;
        }
    }
}

SimpleCardProtocol::SimpleCardProtocol(
    IsLeaderFunction isLeader,
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    impl {
        std::make_shared<Impl>(
            std::move(isLeader), std::move(peerCommandSender))}
{
    assert(impl->cardManager);
    impl->cardManager->subscribe(impl);
}

SimpleCardProtocol::MessageHandlerRange
SimpleCardProtocol::handleGetMessageHandlers()
{
    assert(impl);
    return impl->messageHandlers;
}

SimpleCardProtocol::SocketRange SimpleCardProtocol::handleGetSockets()
{
    return {};
}

std::shared_ptr<CardManager> SimpleCardProtocol::handleGetCardManager()
{
    assert(impl);
    return impl->cardManager;
}

}
}
