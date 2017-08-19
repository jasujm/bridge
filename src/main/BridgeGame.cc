#include "main/BridgeGame.hh"

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Hand.hh"
#include "bridge/Player.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/CardProtocol.hh"
#include "main/Commands.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "Logging.hh"
#include "Observer.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cassert>
#include <set>
#include <vector>

namespace Bridge {
namespace Main {

using Engine::BridgeEngine;
using Messaging::JsonSerializer;

class BridgeGame::Impl  :
    public Observer<BridgeEngine::ShufflingCompleted>,
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::BiddingCompleted>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::TrickCompleted>,
    public Observer<BridgeEngine::DummyRevealed>,
    public Observer<BridgeEngine::DealEnded> {
public:
    Impl(const BridgeEngine& engine, zmq::socket_t& eventSocket) :
        engine {engine},
        eventSocket {eventSocket}
    {
    }

private:
    // TODO: Temporary. BridgeGame::Impl should own these.
    const BridgeEngine& engine;
    zmq::socket_t& eventSocket;

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

    void handleNotify(const BridgeEngine::ShufflingCompleted&) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::BiddingCompleted&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::TrickCompleted&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;
};

template<typename... Args>
void BridgeGame::Impl::publish(const std::string& command, Args&&... args)
{
    sendCommand(
        eventSocket, JsonSerializer {}, command, std::forward<Args>(args)...);
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::ShufflingCompleted&)
{
    log(LogLevel::DEBUG, "Shuffling completed");
    publish(DEAL_COMMAND);
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::CallMade& event)
{
    const auto position = engine.getPosition(event.player);
    log(LogLevel::DEBUG, "Call made. Position: %s. Call: %s", position, event.call);
    publish(
        CALL_COMMAND,
        std::tie(POSITION_COMMAND, position),
        std::tie(CALL_COMMAND, event.call));
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::BiddingCompleted&)
{
    log(LogLevel::DEBUG, "Bidding completed");
    publish(BIDDING_COMMAND);
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    const auto hand_position = dereference(engine.getPosition(event.hand));
    const auto card_type = event.card.getType().get();
    log(LogLevel::DEBUG, "Card played. Position: %s. Card: %s", hand_position,
        card_type);
    publish(
        PLAY_COMMAND,
        std::tie(POSITION_COMMAND, hand_position),
        std::tie(CARD_COMMAND, card_type));
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TrickCompleted& event)
{
    const auto position = dereference(engine.getPosition(event.winner));
    log(LogLevel::DEBUG, "Trick completed. Winner: %s", position);
    publish(TRICK_COMMAND, std::tie(WINNER_COMMAND, position));
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DummyRevealed&)
{
    log(LogLevel::DEBUG, "Dummy hand revealed");
    publish(DUMMY_COMMAND);
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    log(LogLevel::DEBUG, "Deal ended");
    publish(DEAL_END_COMMAND);
}

BridgeGame::BridgeGame(
    PositionSet positionsControlled,
    std::shared_ptr<zmq::socket_t> eventSocket) :
    gameManager {std::make_shared<Engine::DuplicateGameManager>()},
    positionsControlled {positionsControlled},
    positionsInUse {std::move(positionsControlled)},
    peerCommandSender {std::make_shared<PeerCommandSender>()},
    eventSocket {std::move(eventSocket)}
{
}

void BridgeGame::initializeCardProtocol(
    std::unique_ptr<CardProtocol> cardProtocol)
{
    engine = std::make_shared<Engine::BridgeEngine>(
        dereference(cardProtocol).getCardManager(), gameManager);
    this->cardProtocol = std::move(cardProtocol);

    impl = std::make_shared<Impl>(*engine, dereference(eventSocket));
    engine->subscribeToShufflingCompleted(impl);
    engine->subscribeToCallMade(impl);
    engine->subscribeToBiddingCompleted(impl);
    engine->subscribeToCardPlayed(impl);
    engine->subscribeToTrickCompleted(impl);
    engine->subscribeToDummyRevealed(impl);
    engine->subscribeToDealEnded(impl);
    startIfReady();
}

const BridgeEngine& BridgeGame::handleGetEngine() const
{
    return dereference(engine);
}

const Engine::DuplicateGameManager& BridgeGame::handleGetGameManager() const
{
    return dereference(gameManager);
}

void BridgeGame::startIfReady()
{
    if (positionsInUse.size() == N_POSITIONS) {
        assert(engine);
        log(LogLevel::DEBUG, "Starting bridge engine");
        cardProtocol->initialize();
        engine->initiate();
    }
}

bool BridgeGame::call(
    const std::string& identity, const Player& player, const Call& call)
{
    assert(engine);
    if (engine->call(player, call)) {
        sendToPeersIfClient(
            identity,
            CALL_COMMAND,
            std::make_pair(std::cref(PLAYER_COMMAND), player.getUuid()),
            std::tie(CALL_COMMAND, call));
        return true;
    }
    return false;
}

bool BridgeGame::play(
    const std::string& identity, const Player& player,
    const boost::optional<CardType>& card,
    const boost::optional<std::size_t>& index)
{
    // Either card or index - but not both - needs to be provided
    if (bool(card) == bool(index)) {
        return false;
    }

    assert(engine);
    if (const auto hand = engine->getHandInTurn()) {
        const auto n_card = index ? index : findFromHand(*hand, *card);
        if (n_card) {
            if (engine->play(player, *hand, *n_card)) {
                const auto player_position = engine->getPosition(player);
                sendToPeersIfClient(
                    identity,
                    PLAY_COMMAND,
                    std::make_pair(std::cref(PLAYER_COMMAND), player.getUuid()),
                    std::tie(INDEX_COMMAND, *n_card));
                return true;
            }
        }
    }
    return false;
}

}
}
