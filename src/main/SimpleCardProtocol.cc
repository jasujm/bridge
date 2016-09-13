#include "main/SimpleCardProtocol.hh"

#include "bridge/BridgeConstants.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Position.hh"
#include "engine/SimpleCardManager.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"

#include <boost/optional/optional.hpp>

#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

namespace {

const auto PEER_COMMAND = std::string {"bridgerp"};
const auto POSITIONS_COMMAND = std::string {"positions"};
const auto DEAL_COMMAND = std::string {"deal"};
const auto CARDS_COMMAND = std::string {"cards"};

}

using CardVector = std::vector<CardType>;

using Engine::CardManager;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::Reply;
using Messaging::success;

class SimpleCardProtocol::Impl :
    public Bridge::Observer<CardManager::ShufflingState> {
public:

    Impl(std::shared_ptr<PeerCommandSender> peerCommandSender);

    void setAcceptor(std::weak_ptr<PeerAcceptor> acceptor);

    const std::shared_ptr<Engine::SimpleCardManager> cardManager {
        std::make_shared<Engine::SimpleCardManager>()};

    const std::array<MessageHandlerRange::value_type, 2> messageHandlers {{
        {
            PEER_COMMAND,
            makeMessageHandler(
                *this, &Impl::peer, JsonSerializer {},
                std::make_tuple(POSITIONS_COMMAND))
        },
        {
            DEAL_COMMAND,
            Messaging::makeMessageHandler(
                *this, &Impl::deal, JsonSerializer {},
                std::make_tuple(CARDS_COMMAND))
        },
    }};

private:

    void handleNotify(const CardManager::ShufflingState& state) override;

    Reply<> peer(const std::string& identity, const PositionVector& positions);
    Reply<> deal(const std::string& identity, const CardVector& cards);

    bool expectingCards {false};
    boost::optional<std::string> leaderIdentity;
    std::weak_ptr<PeerAcceptor> peerAcceptor;
    const std::shared_ptr<PeerCommandSender> peerCommandSender;
};

SimpleCardProtocol::Impl::Impl(
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    peerCommandSender {std::move(peerCommandSender)}
{
}

void SimpleCardProtocol::Impl::setAcceptor(std::weak_ptr<PeerAcceptor> acceptor)
{
    peerAcceptor = std::move(acceptor);
}

void SimpleCardProtocol::Impl::handleNotify(
    const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::REQUESTED) {
        if (!leaderIdentity) {
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
                std::tie(CARDS_COMMAND, cards));
        } else {
            expectingCards = true;
        }
    }
}

Reply<> SimpleCardProtocol::Impl::peer(
    const std::string& identity, const PositionVector& positions)
{
    const auto acceptor = std::shared_ptr<PeerAcceptor>(peerAcceptor);
    const auto accept_state = acceptor->acceptPeer(identity, positions);
    if (accept_state != PeerAcceptState::REJECTED) {
        if (std::find(
                positions.begin(), positions.end(), Position::NORTH) !=
            positions.end()) {
            leaderIdentity = identity;
        }
        return success();
    }
    return failure();
}

Reply<> SimpleCardProtocol::Impl::deal(
    const std::string& identity, const CardVector& cards)
{
    if (expectingCards && leaderIdentity == identity) {
        cardManager->shuffle(cards.begin(), cards.end());
        expectingCards = false;
        return success();
    }
    return failure();
}

SimpleCardProtocol::SimpleCardProtocol(
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    impl {
        std::make_shared<Impl>(std::move(peerCommandSender))}
{
    assert(impl->cardManager);
    impl->cardManager->subscribe(impl);
}

void SimpleCardProtocol::handleSetAcceptor(std::weak_ptr<PeerAcceptor> acceptor)
{
    assert(impl);
    impl->setAcceptor(std::move(acceptor));
}

CardProtocol::MessageHandlerRange
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
