#include "main/SimpleCardProtocol.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/CardShuffle.hh"
#include "bridge/CardType.hh"
#include "bridge/Position.hh"
#include "engine/SimpleCardManager.hh"
#include "main/Commands.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Logging.hh"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

using CardVector = std::vector<CardType>;

using Engine::CardManager;
using Messaging::failure;
using Messaging::Identity;
using Messaging::JsonSerializer;
using Messaging::Reply;
using Messaging::success;

class SimpleCardProtocol::Impl :
    public Bridge::Observer<CardManager::ShufflingState> {
public:

    Impl(
        const Uuid& gameUuid,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

    bool acceptPeer(
        const Identity& identity, const PositionVector& positions);

    Reply<> deal(const Identity& identity, const CardVector& cards);

    const Uuid gameUuid;
    const std::shared_ptr<Engine::SimpleCardManager> cardManager {
        std::make_shared<Engine::SimpleCardManager>()};

private:

    void handleNotify(const CardManager::ShufflingState& state) override;

    bool expectingCards {false};
    std::optional<Identity> leaderIdentity;
    const std::shared_ptr<PeerCommandSender> peerCommandSender;
};

SimpleCardProtocol::Impl::Impl(
    const Uuid& gameUuid,
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    gameUuid {gameUuid},
    peerCommandSender {std::move(peerCommandSender)}
{
}

void SimpleCardProtocol::Impl::handleNotify(
    const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::REQUESTED) {
        if (!leaderIdentity) {
            log(LogLevel::DEBUG,
                "Simple card protocol: Generating deck");
            auto cards = generateShuffledDeck();
            assert(cardManager);
            cardManager->shuffle(cards.begin(), cards.end());
            dereference(peerCommandSender).sendCommand(
                JsonSerializer {},
                DEAL_COMMAND,
                std::pair {GAME_COMMAND, gameUuid},
                std::pair {CARDS_COMMAND, std::move(cards)});
        } else {
            log(LogLevel::DEBUG,
                "Simple card protocol: Expecting deck");
            expectingCards = true;
        }
    }
}

bool SimpleCardProtocol::Impl::acceptPeer(
    const Identity& identity, const PositionVector& positions)
{
    if (std::find(positions.begin(), positions.end(), Positions::NORTH) !=
        positions.end()) {
        leaderIdentity = identity;
    }
    return true;
}

Reply<> SimpleCardProtocol::Impl::deal(
    const Identity& identity, const CardVector& cards)
{
    log(LogLevel::DEBUG, "Deal command from %s", identity);
    if (expectingCards && leaderIdentity == identity) {
        cardManager->shuffle(cards.begin(), cards.end());
        expectingCards = false;
        return success();
    }
    return failure();
}

SimpleCardProtocol::SimpleCardProtocol(
    const Uuid& gameUuid,
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    impl {
        std::make_shared<Impl>(gameUuid, std::move(peerCommandSender))}
{
    assert(impl->cardManager);
    impl->cardManager->subscribe(impl);
}

bool SimpleCardProtocol::handleAcceptPeer(
    const Identity& identity, const PositionVector& positions,
    const OptionalArgs&)
{
    assert(impl);
    return impl->acceptPeer(identity, positions);
}

void SimpleCardProtocol::handleInitialize()
{
}

std::shared_ptr<Messaging::MessageHandler>
SimpleCardProtocol::handleGetDealMessageHandler()
{
    return Messaging::makeMessageHandler(
        *impl, &Impl::deal, JsonSerializer {},
        std::tuple {CARDS_COMMAND});
}

SimpleCardProtocol::SocketVector SimpleCardProtocol::handleGetSockets()
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
