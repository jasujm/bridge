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
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "HexUtility.hh"
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

class SimpleCardProtocol::DealCommandDispatcher {
public:

    void register_(
        const Uuid& gameUuid,
        std::shared_ptr<SimpleCardProtocol::Impl> protocol);
    void unregister(const Uuid& gameUuid);

    Reply<> deal(
        const Identity& identity, const Uuid& gameUuid,
        const CardVector& cards);

private:
    std::map<Uuid, std::shared_ptr<SimpleCardProtocol::Impl>> protocols;
};

SimpleCardProtocol::DealCommandDispatcher SimpleCardProtocol::dispatcher;

void SimpleCardProtocol::DealCommandDispatcher::register_(
    const Uuid& gameUuid, std::shared_ptr<SimpleCardProtocol::Impl> protocol)
{
    protocols.try_emplace(gameUuid, std::move(protocol));
}

void SimpleCardProtocol::DealCommandDispatcher::unregister(const Uuid& gameUuid)
{
    protocols.erase(gameUuid);
}

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
            const auto cards = generateShuffledDeck();
            assert(cardManager);
            cardManager->shuffle(cards.begin(), cards.end());
            dereference(peerCommandSender).sendCommand(
                JsonSerializer {},
                DEAL_COMMAND,
                std::tie(GAME_COMMAND, gameUuid),
                std::tie(CARDS_COMMAND, cards));
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
    if (std::find(positions.begin(), positions.end(), Position::NORTH) !=
        positions.end()) {
        leaderIdentity = identity;
    }
    return true;
}

Reply<> SimpleCardProtocol::Impl::deal(
    const Identity& identity, const CardVector& cards)
{
    log(LogLevel::DEBUG, "Deal command from %s", formatHex(identity));
    if (expectingCards && leaderIdentity == identity) {
        cardManager->shuffle(cards.begin(), cards.end());
        expectingCards = false;
        return success();
    }
    return failure();
}

Reply<> SimpleCardProtocol::DealCommandDispatcher::deal(
    const Identity& identity, const Uuid& gameUuid, const CardVector& cards)
{
    const auto iter = protocols.find(gameUuid);
    if (iter != protocols.end()) {
        return dereference(iter->second).deal(identity, cards);
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
    dispatcher.register_(gameUuid, impl);
}

SimpleCardProtocol::~SimpleCardProtocol()
{
    dispatcher.unregister(impl->gameUuid);
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

CardProtocol::MessageHandlerVector
SimpleCardProtocol::handleGetMessageHandlers()
{
    static const auto ret = MessageHandlerVector {
        {
            stringToBlob(DEAL_COMMAND),
            Messaging::makeMessageHandler(
                dispatcher, &DealCommandDispatcher::deal, JsonSerializer {},
                std::make_tuple(GAME_COMMAND, CARDS_COMMAND))
        }
    };
    return ret;
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
