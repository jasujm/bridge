#include "main/BridgeGame.hh"

#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/Deal.hh"
#include "bridge/DuplicateScoring.hh"
#include "bridge/Hand.hh"
#include "bridge/Player.hh"
#include "bridge/Position.hh"
#include "engine/BridgeEngine.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "main/BridgeGameRecorder.hh"
#include "main/CardProtocol.hh"
#include "main/Commands.hh"
#include "main/GameStateHelper.hh"
#include "main/PeerCommandSender.hh"
#include "messaging/CallbackScheduler.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/ContractJsonSerializer.hh"
#include "messaging/DuplicateResultJsonSerializer.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/VulnerabilityJsonSerializer.hh"
#include "messaging/UuidJsonSerializer.hh"
#include "Enumerate.hh"
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

auto getDealResults(const Uuid& gameUuid, BridgeGameRecorder& recorder)
{
    auto ret = nlohmann::json::array();
    for (const auto& deal_result : recorder.recallDealResults(gameUuid)) {
        ret.emplace_back(
            nlohmann::json {
                {DEAL_COMMAND, deal_result.dealUuid},
                {RESULT_COMMAND, deal_result.result},
            });
    }
    return ret;
}

class BridgeGame::Impl  :
    public Observer<BridgeEngine::DealStarted>,
    public Observer<BridgeEngine::TurnStarted>,
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::BiddingCompleted>,
    public Observer<BridgeEngine::TrickStarted>,
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
        std::shared_ptr<Engine::GameManager> gameManager,
        std::shared_ptr<PeerCommandSender> peerCommandSender,
        std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
        IdentitySet participants,
        std::shared_ptr<BridgeGameRecorder> recorder,
        std::unique_ptr<const Deal> deal,
        const PositionPlayerMap& players);

    bool addPeer(const Identity& identity, const nlohmann::json& args);

    std::optional<Position> getPositionForPlayerToJoin(
        const Identity& identity, const std::optional<Position>& position,
        const Player& player) const;

    bool join(
        const Identity& identity, Position position,
        std::shared_ptr<Player> player);

    GameState getState(
        const Player* player,
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

    CardProtocol& getCardProtocol();

private:

    template<typename... Args>
    void sendToPeers(std::string_view command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfClient(
        const Identity& identity, std::string_view command,
        Args&&... args);

    template<typename... Args>
    void publish(std::string_view command, Args&&... args);

    void startDeal();
    void recordGame();
    void recordCurrentDeal();
    void recordCall(const Uuid& dealUuid, const Call& call);
    void recordTrick(const Uuid& dealUuid, Position leaderPosition);
    void recordCard(const Uuid& dealUuid, const CardType& card);

    void handleNotify(const BridgeEngine::DealStarted&) override;
    void handleNotify(const BridgeEngine::TurnStarted&) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::BiddingCompleted&) override;
    void handleNotify(const BridgeEngine::TrickStarted&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::TrickCompleted&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;

    const Uuid uuid;
    std::map<Identity, PositionVector> peers;
    const PositionSet positionsControlled;
    PositionSet positionsInUse;
    std::shared_ptr<PeerCommandSender> peerCommandSender;
    Messaging::SharedSocket eventSocket;
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler;
    const IdentitySet participants;
    BridgeEngine engine;
    std::unique_ptr<CardProtocol> cardProtocol;
    std::uint64_t counter;
    std::shared_ptr<BridgeGameRecorder> recorder;
};

BridgeGame::Impl::Impl(
    const Uuid& uuid,
    PositionSet positionsControlled,
    Messaging::SharedSocket eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<Engine::GameManager> gameManager,
    std::shared_ptr<PeerCommandSender> peerCommandSender,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    IdentitySet participants,
    std::shared_ptr<BridgeGameRecorder> recorder,
    std::unique_ptr<const Deal> deal,
    const PositionPlayerMap& players) :
    uuid {uuid},
    positionsControlled {positionsControlled},
    positionsInUse {std::move(positionsControlled)},
    peerCommandSender {std::move(peerCommandSender)},
    eventSocket {std::move(eventSocket)},
    callbackScheduler {std::move(callbackScheduler)},
    participants {std::move(participants)},
    engine {
        dereference(cardProtocol).getCardManager(),
        std::move(gameManager), std::move(deal)},
    cardProtocol {std::move(cardProtocol)},
    counter {},
    recorder {std::move(recorder)}
{
    for (auto&& [position, player] : players) {
        engine.setPlayer(position, player);
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
    ++counter;
    sendEventMessage(
        dereference(eventSocket), JsonSerializer {}, os.str(),
        std::forward<Args>(args)...,
        std::pair {COUNTER_COMMAND, counter});
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
    for (const auto position : *positions) {
        if (positionsInUse.find(position) != positionsInUse.end()) {
            return false;
        }
    }
    peers[identity] = *positions;
    for (const auto position : *positions) {
        positionsInUse.insert(position);
    }
    if (dereference(cardProtocol).acceptPeer(
            identity, *positions, CardProtocol::OptionalArgs {args})) {
        startIfReady();
        return true;
    }
    return false;
}

std::optional<Position> BridgeGame::Impl::getPositionForPlayerToJoin(
    const Identity& identity, const std::optional<Position>& position,
    const Player& player) const
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
        const auto player_at_position = engine.getPlayer(*position);
        return (!player_at_position || player_at_position == &player) ?
            position : std::nullopt;
    }
    // Otherwise find a position with the following preferred order: 1) a
    // position where the player is already seated, 2) first empty position, 3)
    // none if not found
    auto first_empty_position = std::optional<Position> {};
    for (const auto p : positionsControlled) {
        const auto player_at_position = engine.getPlayer(p);
        if (!first_empty_position && !player_at_position) {
            first_empty_position = p;
        } else if (player_at_position == &player) {
            return p;
        }
    }
    return first_empty_position;
}

bool BridgeGame::Impl::join(
    const Identity& identity, const Position position,
    std::shared_ptr<Player> player)
{
    if (engine.setPlayer(position, player)) {
        recordGame();
        const auto player_uuid = dereference(player).getUuid();
        publish(
            JOIN_COMMAND,
            std::pair {POSITION_COMMAND, position},
            std::pair {PLAYER_COMMAND, player_uuid});
        sendToPeersIfClient(
            identity, JOIN_COMMAND,
            std::pair {GAME_COMMAND, uuid},
            std::pair {PLAYER_COMMAND, player_uuid},
            std::pair {POSITION_COMMAND, position});
        return true;
    }
    return false;
}

GameState BridgeGame::Impl::getState(
    const Player* player,
    const std::optional<std::vector<std::string>>& keys) const
{
    auto game_state = getGameState(
        engine.getCurrentDeal(),
        player ? engine.getPosition(*player) : std::nullopt,
        player && engine.getPlayerInTurn() == player,
        keys);
    if (keys) {
        for (const auto& key : *keys) {
            if (key == RESULTS_COMMAND) {
                game_state.emplace(
                    RESULTS_COMMAND,
                    recorder ? getDealResults(uuid, *recorder) :
                        nlohmann::json::array());
            } else if (key == PLAYERS_COMMAND) {
                auto players = nlohmann::json::object();
                for (const auto position : Position::all()) {
                    const auto player = engine.getPlayer(position);
                    players.emplace(
                        position.value(),
                        player ? nlohmann::json(player->getUuid()) :
                            nlohmann::json());
                }
                game_state.emplace(PLAYERS_COMMAND, std::move(players));
            }
        }
    }
    return game_state;
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
        dereference(cardProtocol).initialize();
        startDeal();
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

CardProtocol& BridgeGame::Impl::getCardProtocol()
{
    return dereference(cardProtocol);
}

void BridgeGame::Impl::startDeal()
{
    log(LogLevel::DEBUG, "Starting deal in game %r", uuid);
    engine.startDeal();
    recordGame();
    recordCurrentDeal();
}

void BridgeGame::Impl::recordGame()
{
    if (recorder) {
        log(LogLevel::DEBUG, "Recording game %r", uuid);
        const auto* deal = engine.getCurrentDeal();
        auto game_state = BridgeGameRecorder::GameState {};
        if (deal) {
            game_state.dealUuid = deal->getUuid();
        }
        for (const auto [n, position] : enumerate(Position::all())) {
            if (const auto* player = engine.getPlayer(position)) {
                game_state.playerUuids[n] = player->getUuid();
            }
        }
        recorder->recordGame(uuid, game_state);
    }
}

void BridgeGame::Impl::recordCurrentDeal() {
    if (recorder) {
        if (const auto* deal = engine.getCurrentDeal()) {
            log(LogLevel::DEBUG, "Recording deal %r", deal->getUuid());
            recorder->recordDeal(*deal);
        }
    }
}

void BridgeGame::Impl::recordCall(const Uuid& dealUuid, const Call& call)
{
    if (recorder) {
        log(LogLevel::DEBUG, "Recording call %r in deal %r", call, dealUuid);
        recorder->recordCall(dealUuid, call);
    }
}

void BridgeGame::Impl::recordTrick(
    const Uuid& dealUuid, const Position leaderPosition)
{
    if (recorder) {
        log(LogLevel::DEBUG,
            "Recording trick, leader %r, in deal %r", leaderPosition, dealUuid);
        recorder->recordTrick(dealUuid, leaderPosition);
    }
}

void BridgeGame::Impl::recordCard(const Uuid& dealUuid, const CardType& card)
{
    if (recorder) {
        log(LogLevel::DEBUG,
            "Recording trick, card %r, in deal %r", card, dealUuid);
        recorder->recordCard(dealUuid, card);
    }
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealStarted& event)
{
    log(LogLevel::DEBUG, "Deal started. Opener: %s. Vulnerability: %s",
        event.opener, event.vulnerability);
    recordGame();
    if (recorder) {
        log(LogLevel::DEBUG, "Recording deal start. Game %s. Deal: %s",
            uuid, event.uuid);
        recorder->recordDealStarted(uuid, event.uuid);
    }
    publish(
        DEAL_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {OPENER_COMMAND, event.opener},
        std::pair {VULNERABILITY_COMMAND, event.vulnerability});
    publish(
        TURN_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {POSITION_COMMAND, event.opener});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TurnStarted& event)
{
    log(LogLevel::DEBUG, "Turn started. Position: %s", event.position);
    publish(
        TURN_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {POSITION_COMMAND, event.position});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::CallMade& event)
{
    log(LogLevel::DEBUG, "Call made. Position: %s. Call: %s",
        event.position, event.call);
    recordCall(event.uuid, event.call);
    publish(
        CALL_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {POSITION_COMMAND, event.position},
        std::pair {CALL_COMMAND, event.call});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::BiddingCompleted& event)
{
    log(LogLevel::DEBUG, "Bidding completed. Declarer: %s. Contract: %s",
        event.declarer, event.contract);
    publish(
        BIDDING_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {DECLARER_COMMAND, event.declarer},
        std::pair {CONTRACT_COMMAND, event.contract});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TrickStarted& event)
{
    log(LogLevel::DEBUG, "Trick started. Leader: %s", event.leader);
    recordTrick(event.uuid, event.leader);
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    const auto card_type = dereference(event.card.getType());
    log(LogLevel::DEBUG, "Card played. Position: %s. Card: %s", event.position,
        card_type);
    recordCard(event.uuid, card_type);
    publish(
        PLAY_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {POSITION_COMMAND, event.position},
        std::pair {CARD_COMMAND, card_type});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::TrickCompleted& event)
{
    log(LogLevel::DEBUG, "Trick completed. Winner: %s", event.winner);
    publish(
        TRICK_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {WINNER_COMMAND, event.winner});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DummyRevealed& event)
{
    log(LogLevel::DEBUG, "Dummy hand revealed");
    publish(
        DUMMY_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {POSITION_COMMAND, event.position},
        std::pair {CARDS_COMMAND, getCardsFromHand(event.hand)});
}

void BridgeGame::Impl::handleNotify(const BridgeEngine::DealEnded& event)
{
    log(LogLevel::DEBUG, "Deal ended");
    const auto& result =
        std::experimental::any_cast<const DuplicateResult&>(event.result);
    if (recorder) {
        log(LogLevel::DEBUG, "Recording deal ended. Game %s",
            uuid, event.uuid);
        recorder->recordDealEnded(uuid, result);
    }
    publish(
        DEAL_END_COMMAND,
        std::pair {DEAL_COMMAND, event.uuid},
        std::pair {
            CONTRACT_COMMAND,
                event.contract ? std::optional {*event.contract} : std::nullopt},
        std::pair {TRICKS_WON_COMMAND, event.tricksWon},
        std::pair {RESULT_COMMAND, result});
    dereference(callbackScheduler).callSoon(&Impl::startDeal, this);
}

BridgeGame::BridgeGame(
    const Uuid& uuid,
    PositionSet positionsControlled,
    Messaging::SharedSocket eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<Engine::GameManager> gameManager,
    std::shared_ptr<PeerCommandSender> peerCommandSender,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    IdentitySet participants,
    std::shared_ptr<BridgeGameRecorder> recorder,
    std::unique_ptr<const Deal> deal,
    const PositionPlayerMap& players) :
    impl {
        std::make_shared<Impl>(
            uuid, std::move(positionsControlled), std::move(eventSocket),
            std::move(cardProtocol), std::move(gameManager),
            std::move(peerCommandSender), std::move(callbackScheduler),
            std::move(participants), std::move(recorder), std::move(deal),
            players)}
{
    auto& engine = impl->getEngine();
    engine.subscribeToDealStarted(impl);
    engine.subscribeToTurnStarted(impl);
    engine.subscribeToCallMade(impl);
    engine.subscribeToBiddingCompleted(impl);
    engine.subscribeToTrickStarted(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToTrickCompleted(impl);
    engine.subscribeToDummyRevealed(impl);
    engine.subscribeToDealEnded(impl);
    impl->startIfReady();
}

BridgeGame::BridgeGame(
    const Uuid& uuid, Messaging::SharedSocket eventSocket,
    std::unique_ptr<CardProtocol> cardProtocol,
    std::shared_ptr<Engine::GameManager> gameManager,
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler,
    std::shared_ptr<BridgeGameRecorder> recorder,
    std::unique_ptr<const Deal> deal,
    const PositionPlayerMap& players) :
    BridgeGame {
        uuid, PositionSet(Position::begin(), Position::end()),
        std::move(eventSocket), std::move(cardProtocol), std::move(gameManager),
        nullptr, callbackScheduler, {}, std::move(recorder), std::move(deal),
        players}
{
}

bool BridgeGame::addPeer(
    const Identity& identity, const nlohmann::json& args)
{
    assert(impl);
    return impl->addPeer(identity, args);
}

std::optional<Position> BridgeGame::getPositionForPlayerToJoin(
    const Identity& identity, const std::optional<Position>& position,
    const Player& player) const
{
    assert(impl);
    return impl->getPositionForPlayerToJoin(identity, position, player);
}

bool BridgeGame::join(
    const Identity& identity, const Position position,
    std::shared_ptr<Player> player)
{
    assert(impl);
    return impl->join(identity, position, std::move(player));
}

GameState BridgeGame::getState(
    const Player* player,
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

CardProtocol& BridgeGame::getCardProtocol()
{
    assert(impl);
    return impl->getCardProtocol();
}

}
}
