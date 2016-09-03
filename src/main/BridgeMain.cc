#include "main/BridgeMain.hh"

#include "bridge/AllowedCalls.hh"
#include "bridge/AllowedCards.hh"
#include "bridge/BasicPlayer.hh"
#include "bridge/Call.hh"
#include "bridge/DealState.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/BridgeEngine.hh"
#include "engine/CardManager.hh"
#include "engine/MakeDealState.hh"
#include "main/PeerClientControl.hh"
#include "main/PeerCommandSender.hh"
#include "main/SimpleCardProtocol.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"
#include "messaging/CommandUtility.hh"
#include "messaging/DealStateJsonSerializer.hh"
#include "messaging/DuplicateScoreSheetJsonSerializer.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/FunctionMessageHandler.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/MessageLoop.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/PositionJsonSerializer.hh"
#include "scoring/DuplicateScoreSheet.hh"
#include "Observer.hh"

#include <boost/iterator/indirect_iterator.hpp>
#include <zmq.hpp>

#include <string>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Main {

using Engine::BridgeEngine;
using Engine::CardManager;
using Messaging::failure;
using Messaging::JsonSerializer;
using Messaging::makeMessageHandler;
using Messaging::MessageQueue;
using Messaging::Reply;
using Messaging::success;
using Scoring::DuplicateScoreSheet;

namespace {

using CallVector = std::vector<Call>;
using CardVector = std::vector<CardType>;
using PositionVector = std::vector<Position>;
using StringVector = std::vector<std::string>;

const auto HELLO_COMMAND = std::string {"bridgehlo"};
const auto PEER_COMMAND = std::string {"bridgerp"};
const auto DEAL_COMMAND = std::string {"deal"};
const auto GET_COMMAND = std::string {"get"};
const auto KEYS_COMMAND = std::string {"keys"};
const auto STATE_COMMAND = std::string {"state"};
const auto ALLOWED_CALLS_COMMAND = std::string {"allowedCalls"};
const auto ALLOWED_CARDS_COMMAND = std::string {"allowedCards"};
const auto SCORE_COMMAND = std::string {"score"};
const auto CALL_COMMAND = std::string {"call"};
const auto PLAY_COMMAND = std::string {"play"};
const auto DEAL_END_COMMAND = std::string {"dealend"};
const auto POSITIONS_COMMAND = std::string {"positions"};
const auto POSITION_COMMAND = std::string {"position"};
const auto CARD_COMMAND = std::string {"card"};

template<typename PairIterator>
auto secondIterator(PairIterator iter)
{
    return boost::make_transform_iterator(
        iter, [](auto&& pair) -> decltype(auto) { return pair.second; });
}

}

class BridgeMain::Impl :
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::DealEnded>,
    public Observer<CardManager::ShufflingState> {
public:

    Impl(
        zmq::context_t& context, const std::string& baseEndpoint,
        PositionRange positions, EndpointRange peerEndpoints);

    void run();

    void terminate();

    BridgeEngine& getEngine();
    CardManager& getCardManager();

    template<typename PositionIterator>
    auto positionPlayerIterator(PositionIterator iter);

    void startIfReady();

private:

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfSelfControlledPlayer(
        const Player& player, const std::string& command, Args&&... args);

    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;
    void handleNotify(const CardManager::ShufflingState& state) override;

    const Player* getPlayerFor(
        const std::string& identity, const boost::optional<Position>& position);

    Reply<Position> hello(const std::string& indentity);
    Reply<> peer(const std::string& identity, const PositionVector& positions);
    Reply<
        boost::optional<DealState>, boost::optional<CallVector>,
        boost::optional<CardVector>, boost::optional<DuplicateScoreSheet>> get(
        const std::string& identity, const boost::optional<Position>& position,
        const StringVector& keys);
    Reply<> call(
        const std::string& identity, const boost::optional<Position>& position,
        const Call& call);
    Reply<> play(
        const std::string& identity, const boost::optional<Position>& position,
        const CardType& card);

    std::shared_ptr<Engine::DuplicateGameManager> gameManager {
        std::make_shared<Engine::DuplicateGameManager>()};
    std::map<Position, std::shared_ptr<BasicPlayer>> players {
        {Position::NORTH, std::make_shared<BasicPlayer>()},
        {Position::EAST, std::make_shared<BasicPlayer>()},
        {Position::SOUTH, std::make_shared<BasicPlayer>()},
        {Position::WEST, std::make_shared<BasicPlayer>()}};
    PeerClientControl peerClientControl;
    std::shared_ptr<PeerCommandSender> peerCommandSender {
        std::make_shared<PeerCommandSender>()};
    SimpleCardProtocol cardProtocol {
        [this](const auto identity)
        {
            const auto& leader = players.at(Position::NORTH);
            assert(leader);
            if (identity) {
                return peerClientControl.isAllowedToAct(*identity, *leader);
            }
            return peerClientControl.isSelfControlledPlayer(*leader);
        },
        peerCommandSender
    };
    BridgeEngine engine {
        cardProtocol.getCardManager(), gameManager,
        secondIterator(players.begin()),
        secondIterator(players.end())};
    zmq::socket_t eventSocket;
    Messaging::MessageLoop messageLoop;
};

template<typename PositionIterator>
auto BridgeMain::Impl::positionPlayerIterator(PositionIterator iter)
{
    return boost::make_transform_iterator(
        iter,
        [&players = this->players](const auto position) -> const Player&
        {
            const auto& player = players.at(position);
            assert(player);
            return *player;
        });
}

BridgeMain::Impl::Impl(
    zmq::context_t& context, const std::string& baseEndpoint,
    PositionRange positions, EndpointRange peerEndpoints) :
    peerClientControl {
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end())},
    eventSocket {context, zmq::socket_type::pub}
{
    auto endpointIterator = Messaging::EndpointIterator {baseEndpoint};
    auto controlSocket = std::make_shared<zmq::socket_t>(
        context, zmq::socket_type::router);
    controlSocket->bind(*endpointIterator++);
    eventSocket.bind(*endpointIterator);
    auto handlers = MessageQueue::HandlerMap {
        {
            HELLO_COMMAND,
            makeMessageHandler(
                *this, &Impl::hello, JsonSerializer {},
                std::make_tuple(),
                std::make_tuple(POSITION_COMMAND))
        },
        {
            PEER_COMMAND,
            makeMessageHandler(
                *this, &Impl::peer, JsonSerializer {},
                std::make_tuple(POSITIONS_COMMAND))
        },
        {
            GET_COMMAND,
            makeMessageHandler(
                *this, &Impl::get, JsonSerializer {},
                std::make_tuple(POSITION_COMMAND, KEYS_COMMAND),
                std::make_tuple(
                    STATE_COMMAND, ALLOWED_CALLS_COMMAND,
                    ALLOWED_CARDS_COMMAND, SCORE_COMMAND))
        },
        {
            CALL_COMMAND,
            makeMessageHandler(
                *this, &Impl::call, JsonSerializer {},
                std::make_tuple(POSITION_COMMAND, CALL_COMMAND))
        },
        {
            PLAY_COMMAND,
            makeMessageHandler(
                *this, &Impl::play, JsonSerializer {},
                std::make_tuple(POSITION_COMMAND, CARD_COMMAND))
        }
    };
    for (auto&& handler : cardProtocol.getMessageHandlers()) {
        handlers.emplace(handler);
    }
    messageLoop.addSocket(
        std::move(controlSocket), MessageQueue { std::move(handlers) });
    for (const auto& endpoint : peerEndpoints) {
        messageLoop.addSocket(
            peerCommandSender->addPeer(context, endpoint),
            [&sender = *this->peerCommandSender](zmq::socket_t& socket)
            {
                sender.processReply(socket);
                return true;
            });
    }
    sendToPeers(
        PEER_COMMAND,
        std::make_pair(
            POSITIONS_COMMAND,
            PositionVector(positions.begin(), positions.end())));
}

void BridgeMain::Impl::run()
{
    messageLoop.run();
}

void BridgeMain::Impl::terminate()
{
    messageLoop.terminate();
}

void BridgeMain::Impl::startIfReady()
{
    const auto ready = peerClientControl.arePlayersControlled(
        boost::make_indirect_iterator(secondIterator(players.begin())),
        boost::make_indirect_iterator(secondIterator(players.end())));
    if (ready) {
        engine.initiate();
    }
}

BridgeEngine& BridgeMain::Impl::getEngine()
{
    return engine;
}

CardManager& BridgeMain::Impl::getCardManager()
{
    const auto cardManager = cardProtocol.getCardManager();
    assert(cardManager);
    return *cardManager;
}

template<typename... Args>
void BridgeMain::Impl::publish(const std::string& command, Args&&... args)
{
    sendCommand(
        eventSocket, JsonSerializer {}, command, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeers(
    const std::string& command, Args&&... args)
{
    assert(peerCommandSender);
    peerCommandSender->sendCommand(
        JsonSerializer {}, command, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeMain::Impl::sendToPeersIfSelfControlledPlayer(
    const Player& player, const std::string& command, Args&&... args)
{
    if (peerClientControl.isSelfControlledPlayer(player)) {
        sendToPeers(command, std::forward<Args>(args)...);
    }
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CallMade& event)
{
    const auto position = engine.getPosition(event.player);
    publish(CALL_COMMAND);
    sendToPeersIfSelfControlledPlayer(
        event.player,
        CALL_COMMAND,
        std::make_pair(POSITION_COMMAND, position),
        std::make_pair(CALL_COMMAND, event.call));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    const auto player_position = engine.getPosition(event.player);
    const auto hand_position = engine.getPosition(event.hand);
    const auto card_type = event.card.getType().get();
    publish(
        PLAY_COMMAND,
        std::make_pair(POSITION_COMMAND, std::cref(hand_position)),
        std::make_pair(CARD_COMMAND, std::cref(card_type)));
    sendToPeersIfSelfControlledPlayer(
        event.player,
        PLAY_COMMAND,
        std::make_pair(POSITION_COMMAND, std::cref(player_position)),
        std::make_pair(CARD_COMMAND, std::cref(card_type)));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DealEnded&)
{
    publish(DEAL_END_COMMAND);
}

void BridgeMain::Impl::handleNotify(const CardManager::ShufflingState& state)
{
    if (state == CardManager::ShufflingState::COMPLETED) {
        publish(DEAL_COMMAND);
    }
}

const Player* BridgeMain::Impl::getPlayerFor(
    const std::string& identity, const boost::optional<Position>& position)
{
    if (position) {
        const auto& player = engine.getPlayer(*position);
        if (peerClientControl.isAllowedToAct(identity, player)) {
            return &player;
        }
        return nullptr;
    }
    return peerClientControl.getPlayer(identity);
}

Reply<Position> BridgeMain::Impl::hello(const std::string& identity)
{
    if (const auto player = peerClientControl.addClient(identity)) {
        return success(engine.getPosition(*player));
    }
    return failure();
}

Reply<> BridgeMain::Impl::peer(
    const std::string& identity, const PositionVector& positions)
{
    const auto success_ = peerClientControl.addPeer(
        identity,
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end()));
    if (success_) {
        startIfReady();
        return success();
    }
    return failure();
}

Reply<
    boost::optional<DealState>, boost::optional<CallVector>,
    boost::optional<CardVector>, boost::optional<DuplicateScoreSheet>>
BridgeMain::Impl::get(
    const std::string& identity, const boost::optional<Position>& position,
    const StringVector& keys)
{
    const auto player = getPlayerFor(identity, position);
    if (!player) {
        return failure();
    }
    auto deal_state = boost::optional<DealState> {};
    auto allowed_calls = boost::optional<CallVector> {};
    auto allowed_cards = boost::optional<CardVector> {};
    auto score_sheet = boost::optional<DuplicateScoreSheet> {};
    const auto player_has_turn = (engine.getPlayerInTurn() == player);
    for (auto&& key : keys) {
        if (key == STATE_COMMAND) {
            deal_state = makeDealState(engine, *player);
        } else if (key == ALLOWED_CALLS_COMMAND) {
            allowed_calls.emplace();
            if (player_has_turn) {
                if (const auto bidding = engine.getBidding()) {
                    getAllowedCalls(
                        *bidding, std::back_inserter(*allowed_calls));
                }
            }
        } else if (key == ALLOWED_CARDS_COMMAND) {
            allowed_cards.emplace();
            if (player_has_turn) {
                if (const auto trick = engine.getCurrentTrick()) {
                    getAllowedCards(*trick, std::back_inserter(*allowed_cards));
                }
            }
        } else if (key == SCORE_COMMAND) {
            assert(gameManager);
            score_sheet = gameManager->getScoreSheet();
        }
    }
    return success(
        std::move(deal_state), std::move(allowed_calls),
        std::move(allowed_cards), std::move(score_sheet));
}

Reply<> BridgeMain::Impl::call(
    const std::string& identity, const boost::optional<Position>& position,
    const Call& call)
{
    if (const auto player = getPlayerFor(identity, position)) {
        if (engine.call(*player, call)) {
            return success();
        }
    }
    return failure();
}

Reply<> BridgeMain::Impl::play(
    const std::string& identity, const boost::optional<Position>& position,
    const CardType& card)
{
    if (const auto player = getPlayerFor(identity, position)) {
        if (const auto hand = engine.getHandInTurn()) {
            if (const auto n_card = findFromHand(*hand, card)) {
                if (engine.play(*player, *hand, *n_card)) {
                    return success();
                }
            }
        }
    }
    return failure();
}

BridgeMain::BridgeMain(
    zmq::context_t& context, const std::string& baseEndpoint,
    PositionRange positions, EndpointRange peerEndpoints) :
    impl {
        std::make_shared<Impl>(
            context, baseEndpoint, positions, peerEndpoints)}
{
    auto& engine = impl->getEngine();
    engine.subscribeToCallMade(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToDealEnded(impl);
    impl->getCardManager().subscribe(impl);
    impl->startIfReady();
}

BridgeMain::~BridgeMain() = default;

void BridgeMain::run()
{
    assert(impl);
    impl->run();
}

void BridgeMain::terminate()
{
    assert(impl);
    impl->terminate();
}

}
}
