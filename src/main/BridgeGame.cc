#include "main/BridgeGame.hh"

#include "bridge/Card.hh"
#include "bridge/CardShuffle.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/Hand.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "main/CardProtocol.hh"
#include "main/Commands.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/DuplicateScoreJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "messaging/TricksWonJsonSerializer.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "scoring/DuplicateScore.hh"
#include "Logging.hh"
#include "Observer.hh"
#include "Utility.hh"

#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>
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
    public Observer<BridgeEngine::DealEnded> {
public:

    Impl(
        const Uuid& uuid,
        PositionSet positionsControlled,
        std::shared_ptr<zmq::socket_t> eventSocket,
        std::unique_ptr<CardProtocol> cardProtocol,
        std::shared_ptr<PeerCommandSender> peerCommandSender);

    bool addPeer(const std::string& identity, const nlohmann::json& args);

    boost::optional<Position> getPositionForPlayerToJoin(
        const std::string& identity, const boost::optional<Position>& position);

    void join(
        const std::string& identity, Position position,
        std::shared_ptr<Player> player);

    bool call(
        const std::string& identity, const Player& player, const Call& call);

    bool play(
        const std::string& identity, const Player& player,
        const boost::optional<CardType>& card,
        const boost::optional<std::size_t>& index);

    void startIfReady();

    BridgeEngine& getEngine();

    Engine::DuplicateGameManager& getGameManager();

private:

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfClient(
        const std::string& identity, const std::string& command,
        Args&&... args);

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

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
    std::map<std::string, PositionVector> peers;
    const PositionSet positionsControlled;
    PositionSet positionsInUse;
    std::shared_ptr<PeerCommandSender> peerCommandSender;
    std::shared_ptr<zmq::socket_t> eventSocket;
    std::shared_ptr<Shuffler> shuffler;
    BridgeEngine engine;
    std::unique_ptr<CardProtocol> cardProtocol;
};

BridgeGame::Impl::Impl(
    const Uuid& uuid,
    PositionSet positionsControlled,
    std::shared_ptr<zmq::socket_t> eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    uuid {uuid},
    gameManager {std::make_shared<Engine::DuplicateGameManager>()},
    positionsControlled {positionsControlled},
    positionsInUse {std::move(positionsControlled)},
    peerCommandSender {std::move(peerCommandSender)},
    eventSocket {std::move(eventSocket)},
    shuffler {cardProtocol ? nullptr : std::make_shared<Shuffler>()},
    engine {
        cardProtocol ?
            cardProtocol->getCardManager() : shuffler->getCardManager(),
        gameManager},
    cardProtocol {std::move(cardProtocol)}
{
    if (shuffler) {
        shuffler->getCardManager()->subscribe(shuffler);
    }
}

template<typename... Args>
void BridgeGame::Impl::sendToPeers(
    const std::string& command, Args&&... args)
{
    if (peerCommandSender) {
        log(LogLevel::DEBUG, "Sending command to peers: %s", command);
        peerCommandSender->sendCommand(
            Messaging::JsonSerializer {}, command, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void BridgeGame::Impl::sendToPeersIfClient(
    const std::string& identity, const std::string& command, Args&&... args)
{
    if (peers.find(identity) == peers.end()) {
        sendToPeers(command, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void BridgeGame::Impl::publish(const std::string& command, Args&&... args)
{
    std::ostringstream os;
    os << uuid << ':' << command;
    log(LogLevel::DEBUG, "Publishing event: %s", command);
    sendCommand(
        dereference(eventSocket), JsonSerializer {}, os.str(),
        std::forward<Args>(args)...);
}

bool BridgeGame::Impl::addPeer(
    const std::string& identity, const nlohmann::json& args)
{
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
    if (cardProtocol && cardProtocol->acceptPeer(identity, *positions, args)) {
        startIfReady();
        return true;
    }
    return false;
}

boost::optional<Position> BridgeGame::Impl::getPositionForPlayerToJoin(
    const std::string& identity, const boost::optional<Position>& position)
{
    const auto iter = peers.find(identity);
    // For peers only controlled positions apply
    if (iter != peers.end()) {
        const auto& controlled_positions = iter->second;
        const auto peer_controls_position = position && std::find(
            controlled_positions.begin(), controlled_positions.end(), *position)
            != controlled_positions.end();
        if (!peer_controls_position) {
            return boost::none;
        }
    }
    // For clients try preferred position if given
    if (position) {
        return engine.getPlayer(*position) ? boost::none : position;
    }
    // Otherwise select any position (or none if none is free)
    for (const auto p : positionsControlled) {
        if (!engine.getPlayer(p)) {
            return p;
        }
    }
    return boost::none;
}

void BridgeGame::Impl::join(
    const std::string& identity, const Position position,
    std::shared_ptr<Player> player)
{
    sendToPeersIfClient(
        identity, JOIN_COMMAND,
        std::tie(GAME_COMMAND, uuid),
        std::make_pair(
            std::cref(PLAYER_COMMAND), dereference(player).getUuid()),
        std::tie(POSITION_COMMAND, position));
    engine.setPlayer(position, std::move(player));
}

bool BridgeGame::Impl::call(
    const std::string& identity, const Player& player, const Call& call)
{
    if (engine.call(player, call)) {
        sendToPeersIfClient(
            identity,
            CALL_COMMAND,
            std::tie(GAME_COMMAND, uuid),
            std::make_pair(std::cref(PLAYER_COMMAND), player.getUuid()),
            std::tie(CALL_COMMAND, call));
        return true;
    }
    return false;
}

bool BridgeGame::Impl::play(
    const std::string& identity, const Player& player,
    const boost::optional<CardType>& card,
    const boost::optional<std::size_t>& index)
{
    // Either card or index - but not both - needs to be provided
    if (bool(card) == bool(index)) {
        return false;
    }

    if (const auto hand = engine.getHandInTurn()) {
        const auto n_card = index ? index : findFromHand(*hand, *card);
        if (n_card) {
            if (engine.play(player, *hand, *n_card)) {
                const auto player_position = engine.getPosition(player);
                sendToPeersIfClient(
                    identity,
                    PLAY_COMMAND,
                    std::tie(GAME_COMMAND, uuid),
                    std::make_pair(std::cref(PLAYER_COMMAND), player.getUuid()),
                    std::tie(INDEX_COMMAND, *n_card));
                return true;
            }
        }
    }
    return false;
}

void BridgeGame::Impl::startIfReady()
{
    if (positionsInUse.size() == N_POSITIONS) {
        log(LogLevel::DEBUG, "Starting bridge engine");
        if (cardProtocol) {
            cardProtocol->initialize();
        }
        engine.initiate();
    }
}

BridgeEngine& BridgeGame::Impl::getEngine()
{
    return engine;
}

Engine::DuplicateGameManager& BridgeGame::Impl::getGameManager()
{
    assert(gameManager);
    return *gameManager;
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealStarted& event)
{
    log(LogLevel::DEBUG, "Deal started. Opener: %s. Vulnerability: %s",
        event.opener, event.vulnerability);
    publish(
        DEAL_COMMAND,
        std::tie(OPENER_COMMAND, event.opener),
        std::tie(VULNERABILITY_COMMAND, event.vulnerability));
    publish(
        TURN_COMMAND,
        std::tie(POSITION_COMMAND, event.opener));
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TurnStarted& event)
{
    log(LogLevel::DEBUG, "Turn started. Position: %s", event.position);
    publish(
        TURN_COMMAND,
        std::tie(POSITION_COMMAND, event.position));
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

void BridgeGame::Impl::handleNotify(const BridgeEngine::BiddingCompleted& event)
{
    log(LogLevel::DEBUG, "Bidding completed. Declarer: %s. Contract: %s",
        event.declarer, event.contract);
    publish(
        BIDDING_COMMAND,
        std::tie(DECLARER_COMMAND, event.declarer),
        std::tie(CONTRACT_COMMAND, event.contract));
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

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealEnded& event)
{
    log(LogLevel::DEBUG, "Deal ended. Tricks won: %s", event.tricksWon);
    using ScoreEntry = Engine::DuplicateGameManager::ScoreEntry;
    const auto& result = boost::any_cast<const ScoreEntry&>(event.result);
    publish(
        DEAL_END_COMMAND,
        std::tie(TRICKS_WON_COMMAND, event.tricksWon),
        std::tie(SCORE_COMMAND, result));
}

BridgeGame::BridgeGame(
    const Uuid& uuid,
    PositionSet positionsControlled,
    std::shared_ptr<zmq::socket_t> eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<PeerCommandSender> peerCommandSender) :
    impl {
    std::make_shared<Impl>(
        uuid, std::move(positionsControlled), std::move(eventSocket),
        std::move(cardProtocol), std::move(peerCommandSender))}
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

BridgeGame::BridgeGame(const Uuid& uuid, std::shared_ptr<zmq::socket_t> eventSocket) :
    BridgeGame {
        uuid, PositionSet(POSITIONS.begin(), POSITIONS.end()),
        std::move(eventSocket), nullptr, nullptr}
{
}

bool BridgeGame::addPeer(
    const std::string& identity, const nlohmann::json& args)
{
    assert(impl);
    return impl->addPeer(identity, args);
}

boost::optional<Position> BridgeGame::getPositionForPlayerToJoin(
    const std::string& identity, const boost::optional<Position>& position)
{
    assert(impl);
    return impl->getPositionForPlayerToJoin(identity, position);
}

void BridgeGame::join(
    const std::string& identity, const Position position,
    std::shared_ptr<Player> player)
{
    assert(impl);
    impl->join(identity, position, std::move(player));
}

bool BridgeGame::call(
    const std::string& identity, const Player& player, const Call& call)
{
    assert(impl);
    return impl->call(identity, player, call);
}

bool BridgeGame::play(
    const std::string& identity, const Player& player,
    const boost::optional<CardType>& card,
    const boost::optional<std::size_t>& index)
{
    assert(impl);
    return impl->play(identity, player, card, index);
}

const BridgeEngine& BridgeGame::handleGetEngine() const
{
    assert(impl);
    return impl->getEngine();
}

const Engine::DuplicateGameManager& BridgeGame::handleGetGameManager() const
{
    assert(impl);
    return impl->getGameManager();
}

}
}
