#include "main/BridgeGame.hh"

#include "bridge/Card.hh"
#include "bridge/CardShuffle.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/Hand.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "bridge/UuidGenerator.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "main/CardProtocol.hh"
#include "main/Commands.hh"
#include "main/GameStateHelper.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CallbackScheduler.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/DuplicateScoreJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "scoring/DuplicateScore.hh"
#include "IoUtility.hh"
#include "Logging.hh"
#include "Observer.hh"
#include "Utility.hh"

#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace Bridge {
namespace Main {

using PositionVector = std::vector<Position>;

using Engine::CardManager;
using Engine::SimpleCardManager;
using Engine::BridgeEngine;
using Messaging::Identity;
using Messaging::JsonSerializer;

namespace {

class Shuffler : public Observer<CardManager::ShufflingState> {
public:
    std::shared_ptr<CardManager> getCardManager();
private:
    void handleNotify(const CardManager::ShufflingState& state);
    std::shared_ptr<SimpleCardManager> cardManager {
        std::make_shared<SimpleCardManager>()};
};

std::shared_ptr<CardManager> Shuffler::getCardManager()
{
    return cardManager;
}

void Shuffler::handleNotify(const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::REQUESTED) {
        const auto cards = generateShuffledDeck();
        assert(cardManager);
        cardManager->shuffle(cards.begin(), cards.end());
    }
}

}

class BridgeGame::Impl  :
    public Observer<BridgeEngine::DealStarted>,
    public Observer<BridgeEngine::TurnStarted>,
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::BiddingCompleted>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::TrickCompleted>,
    public Observer<BridgeEngine::DummyRevealed>,
    public Observer<BridgeEngine::DealEnded>  {
public:

    Impl(
        const Uuid& uuid,
        PositionSet positionsControlled,
        Messaging::SharedSocket eventSocket,
        std::unique_ptr<CardProtocol> cardProtocol,
        std::shared_ptr<PeerCommandSender> peerCommandSender,
        std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
        IdentitySet participants);

    bool addPeer(const Identity& identity, const nlohmann::json& args);

    std::optional<Position> getPositionForPlayerToJoin(
        const Identity& identity, const std::optional<Position>& position);

    bool join(
        const Identity& identity, Position position,
        std::shared_ptr<Player> player);

    GameState getState(
        const Player& player,
        const std::optional<std::vector<std::string>>& keys) const;

    Counter getCounter() const;

    bool call(
        const Identity& identity, const Player& player, const Call& call);

    bool play(
        const Identity& identity, const Player& player,
        const std::optional<CardType>& card,
        const std::optional<int>& index);

    void startIfReady();

    BridgeEngine& getEngine();

    PeerCommandSender* getPeerCommandSender();

    CardProtocol* getCardProtocol();

private:

    template<typename... Args>
    void sendToPeers(std::string_view command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfClient(
        const Identity& identity, std::string_view command,
        Args&&... args);

    template<typename... Args>
    void publish(std::string_view command, Args&&... args);

    void handleNotify(const BridgeEngine::DealStarted&) override;
    void handleNotify(const BridgeEngine::TurnStarted&) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::BiddingCompleted&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::TrickCompleted&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;

    const Uuid uuid;
    std::shared_ptr<Engine::DuplicateGameManager> gameManager;
    std::map<Identity, PositionVector> peers;
    const PositionSet positionsControlled;
    PositionSet positionsInUse;
    std::shared_ptr<PeerCommandSender> peerCommandSender;
    Messaging::SharedSocket eventSocket;
    std::shared_ptr<Shuffler> shuffler;
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler;
    const IdentitySet participants;
    BridgeEngine engine;
    std::unique_ptr<CardProtocol> cardProtocol;
    UuidGenerator uuidGenerator;
    Uuid dealUuid;
    std::uint64_t counter;
};

BridgeGame::Impl::Impl(
    const Uuid& uuid,
    PositionSet positionsControlled,
    Messaging::SharedSocket eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<PeerCommandSender> peerCommandSender,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    IdentitySet participants) :
    uuid {uuid},
    gameManager {std::make_shared<Engine::DuplicateGameManager>()},
    positionsControlled {positionsControlled},
    positionsInUse {std::move(positionsControlled)},
    peerCommandSender {std::move(peerCommandSender)},
    eventSocket {std::move(eventSocket)},
    shuffler {cardProtocol ? nullptr : std::make_shared<Shuffler>()},
    callbackScheduler {std::move(callbackScheduler)},
    participants {std::move(participants)},
    engine {
        cardProtocol ?
            cardProtocol->getCardManager() : shuffler->getCardManager(),
        gameManager},
    cardProtocol {std::move(cardProtocol)},
    uuidGenerator {createUuidGenerator()},
    dealUuid {},
    counter {}
{
    if (shuffler) {
        shuffler->getCardManager()->subscribe(shuffler);
    }
}

template<typename... Args>
void BridgeGame::Impl::sendToPeers(
    const std::string_view command, Args&&... args)
{
    if (peerCommandSender) {
        log(LogLevel::DEBUG, "Sending command to peers: %s", command);
        peerCommandSender->sendCommand(
            Messaging::JsonSerializer {}, command, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void BridgeGame::Impl::sendToPeersIfClient(
    const Identity& identity, const std::string_view command, Args&&... args)
{
    if (peers.find(identity) == peers.end()) {
        sendToPeers(command, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void BridgeGame::Impl::publish(const std::string_view command, Args&&... args)
{
    std::ostringstream os;
    os << uuid << ':' << command;
    log(LogLevel::DEBUG, "Publishing event: %s", command);
    sendEventMessage(
        dereference(eventSocket), JsonSerializer {}, os.str(),
        std::pair {DEAL_COMMAND, dealUuid},
        std::forward<Args>(args)...,
        std::pair {COUNTER_COMMAND, counter});
    ++counter;
}

bool BridgeGame::Impl::addPeer(
    const Identity& identity, const nlohmann::json& args)
{
    if (!participants.empty() &&
        participants.find(identity.userId) == participants.end()) {
        return false;
    }
    const auto iter = args.find(POSITIONS_COMMAND);
    if (iter == args.end()) {
        return false;
    }
    const auto positions = Messaging::tryFromJson<PositionVector>(*iter);
    if (!positions) {
        return false;
    }
    if (std::any_of(
            positions->begin(), positions->end(),
            [this](const auto p)
            {
                return positionsInUse.find(p) != positionsInUse.end();
            })) {
        return false;
    }
    peers[identity] = *positions;
    for (const auto position : *positions) {
        positionsInUse.insert(position);
    }
    if (cardProtocol &&
        cardProtocol->acceptPeer(
            identity, *positions, CardProtocol::OptionalArgs {args})) {
        startIfReady();
        return true;
    }
    return false;
}

std::optional<Position> BridgeGame::Impl::getPositionForPlayerToJoin(
    const Identity& identity, const std::optional<Position>& position)
{
    const auto iter = peers.find(identity);
    // For peers only controlled positions apply
    if (iter != peers.end()) {
        const auto& controlled_positions = iter->second;
        const auto peer_controls_position = position && std::find(
            controlled_positions.begin(), controlled_positions.end(), *position)
            != controlled_positions.end();
        if (!peer_controls_position) {
            return std::nullopt;
        }
    }
    // For clients try preferred position if given
    if (position) {
        return engine.getPlayer(*position) ? std::nullopt : position;
    }
    // Otherwise select any position (or none if none is free)
    for (const auto p : positionsControlled) {
        if (!engine.getPlayer(p)) {
            return p;
        }
    }
    return std::nullopt;
}

bool BridgeGame::Impl::join(
    const Identity& identity, const Position position,
    std::shared_ptr<Player> player)
{
    if (engine.setPlayer(position, player)) {
        sendToPeersIfClient(
            identity, JOIN_COMMAND,
            std::pair {GAME_COMMAND, uuid},
            std::pair {PLAYER_COMMAND, dereference(player).getUuid()},
            std::pair {POSITION_COMMAND, position});
        return true;
    }
    return false;
}

GameState BridgeGame::Impl::getState(
    const Player& player,
    const std::optional<std::vector<std::string>>& keys) const
{
    assert(gameManager);
    return getGameState(player, engine, *gameManager, dealUuid, keys);
}

BridgeGame::Counter BridgeGame::Impl::getCounter() const
{
    return counter;
}


bool BridgeGame::Impl::call(
    const Identity& identity, const Player& player, const Call& call)
{
    if (engine.call(player, call)) {
        sendToPeersIfClient(
            identity,
            CALL_COMMAND,
            std::pair {GAME_COMMAND, uuid},
            std::pair {PLAYER_COMMAND, player.getUuid()},
            std::pair {CALL_COMMAND, call});
        return true;
    }
    return false;
}

bool BridgeGame::Impl::play(
    const Identity& identity, const Player& player,
    const std::optional<CardType>& card,
    const std::optional<int>& index)
{
    // Either card or index - but not both - needs to be provided
    if (bool(card) == bool(index)) {
        return false;
    }

    if (const auto hand = engine.getHandInTurn()) {
        const auto n_card = index ? index : findFromHand(*hand, *card);
        if (n_card) {
            if (engine.play(player, *hand, *n_card)) {
                sendToPeersIfClient(
                    identity,
                    PLAY_COMMAND,
                    std::pair {GAME_COMMAND, uuid},
                    std::pair {PLAYER_COMMAND, player.getUuid()},
                    std::pair {INDEX_COMMAND, *n_card});
                return true;
            }
        }
    }
    return false;
}

void BridgeGame::Impl::startIfReady()
{
    if (positionsInUse.size() == Position::size()) {
        log(LogLevel::DEBUG, "Starting bridge engine");
        if (cardProtocol) {
            cardProtocol->initialize();
        }
        engine.startDeal();
    }
}

BridgeEngine& BridgeGame::Impl::getEngine()
{
    return engine;
}

PeerCommandSender* BridgeGame::Impl::getPeerCommandSender()
{
    return peerCommandSender.get();
}

CardProtocol* BridgeGame::Impl::getCardProtocol()
{
    return cardProtocol.get();
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealStarted& event)
{
    log(LogLevel::DEBUG, "Deal started. Opener: %s. Vulnerability: %s",
        event.opener, event.vulnerability);
    dealUuid = uuidGenerator();
    publish(
        DEAL_COMMAND,
        std::pair {OPENER_COMMAND, event.opener},
        std::pair {VULNERABILITY_COMMAND, event.vulnerability});
    publish(
        TURN_COMMAND,
        std::pair {POSITION_COMMAND, event.opener});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TurnStarted& event)
{
    log(LogLevel::DEBUG, "Turn started. Position: %s", event.position);
    publish(
        TURN_COMMAND,
        std::pair {POSITION_COMMAND, event.position});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::CallMade& event)
{
    const auto position = engine.getPosition(event.player);
    log(LogLevel::DEBUG, "Call made. Position: %s. Call: %s", position, event.call);
    publish(
        CALL_COMMAND,
        std::pair {POSITION_COMMAND, position},
        std::pair {CALL_COMMAND, event.call});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::BiddingCompleted& event)
{
    log(LogLevel::DEBUG, "Bidding completed. Declarer: %s. Contract: %s",
        event.declarer, event.contract);
    publish(
        BIDDING_COMMAND,
        std::pair {DECLARER_COMMAND, event.declarer},
        std::pair {CONTRACT_COMMAND, event.contract});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    const auto hand_position = dereference(engine.getPosition(event.hand));
    const auto card_type = dereference(event.card.getType());
    log(LogLevel::DEBUG, "Card played. Position: %s. Card: %s", hand_position,
        card_type);
    publish(
        PLAY_COMMAND,
        std::pair {POSITION_COMMAND, hand_position},
        std::pair {CARD_COMMAND, card_type});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TrickCompleted& event)
{
    const auto position = dereference(engine.getPosition(event.winner));
    log(LogLevel::DEBUG, "Trick completed. Winner: %s", position);
    publish(TRICK_COMMAND, std::pair {WINNER_COMMAND, position});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DummyRevealed& event)
{
    log(LogLevel::DEBUG, "Dummy hand revealed");
    publish(
        DUMMY_COMMAND,
        std::pair {POSITION_COMMAND, event.position},
        std::pair {CARDS_COMMAND, getCardsFromHand(event.hand)});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealEnded& event)
{
    log(LogLevel::DEBUG, "Deal ended");
    using ScoreEntry = Engine::DuplicateGameManager::ScoreEntry;
    const auto& result = std::experimental::any_cast<const ScoreEntry&>(event.result);
    publish(
        DEAL_END_COMMAND,
        std::pair {SCORE_COMMAND, result});
    dereference(callbackScheduler).callSoon(
        &BridgeEngine::startDeal, std::ref(engine));
}

BridgeGame::BridgeGame(
    const Uuid& uuid,
    PositionSet positionsControlled,
    Messaging::SharedSocket eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<PeerCommandSender> peerCommandSender,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    IdentitySet participants) :
    impl {
    std::make_shared<Impl>(
        uuid, std::move(positionsControlled), std::move(eventSocket),
        std::move(cardProtocol), std::move(peerCommandSender),
        std::move(callbackScheduler), std::move(participants))}
{
    auto& engine = impl->getEngine();
    engine.subscribeToDealStarted(impl);
    engine.subscribeToTurnStarted(impl);
    engine.subscribeToCallMade(impl);
    engine.subscribeToBiddingCompleted(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToTrickCompleted(impl);
    engine.subscribeToDummyRevealed(impl);
    engine.subscribeToDealEnded(impl);
    impl->startIfReady();
}

BridgeGame::BridgeGame(
    const Uuid& uuid, Messaging::SharedSocket eventSocket,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler) :
    BridgeGame {
        uuid, PositionSet(Position::begin(), Position::end()),
        std::move(eventSocket), nullptr, nullptr, callbackScheduler, {}}
{
}

bool BridgeGame::addPeer(
    const Identity& identity, const nlohmann::json& args)
{
    assert(impl);
    return impl->addPeer(identity, args);
}

std::optional<Position> BridgeGame::getPositionForPlayerToJoin(
    const Identity& identity, const std::optional<Position>& position)
{
    assert(impl);
    return impl->getPositionForPlayerToJoin(identity, position);
}

bool BridgeGame::join(
    const Identity& identity, const Position position,
    std::shared_ptr<Player> player)
{
    assert(impl);
    return impl->join(identity, position, std::move(player));
}

GameState BridgeGame::getState(
    const Player& player,
    const std::optional<std::vector<std::string>>& keys) const
{
    assert(impl);
    return impl->getState(player, keys);
}

BridgeGame::Counter BridgeGame::getCounter() const
{
    assert(impl);
    return impl->getCounter();
}

bool BridgeGame::call(
    const Identity& identity, const Player& player, const Call& call)
{
    assert(impl);
    return impl->call(identity, player, call);
}

bool BridgeGame::play(
    const Identity& identity, const Player& player,
    const std::optional<CardType>& card,
    const std::optional<int>& index)
{
    assert(impl);
    return impl->play(identity, player, card, index);
}

PeerCommandSender* BridgeGame::getPeerCommandSender()
{
    assert(impl);
    return impl->getPeerCommandSender();
}

CardProtocol* BridgeGame::getCardProtocol()
{
    assert(impl);
    return impl->getCardProtocol();
}

}
}
