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
const auto DUMMY_COMMAND = std::string {"dummy"};
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
    public CardProtocol::PeerAcceptor,
    public Observer<BridgeEngine::CallMade>,
    public Observer<BridgeEngine::CardPlayed>,
    public Observer<BridgeEngine::DummyRevealed>,
    public Observer<BridgeEngine::DealEnded>,
    public Observer<CardManager::ShufflingState> {
public:

    Impl(
        zmq::context_t& context, const std::string& baseEndpoint,
        PositionRange positions, EndpointRange peerEndpoints);

    void run();

    void terminate();

    CardProtocol& getCardProtocol();
    BridgeEngine& getEngine();

    template<typename PositionIterator>
    auto positionPlayerIterator(PositionIterator iter);

    bool readyToStart() const;
    void startIfReady();

private:

    template<typename... Args>
    void publish(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeers(const std::string& command, Args&&... args);

    template<typename... Args>
    void sendToPeersIfSelfControlledPlayer(
        const Player& player, const std::string& command, Args&&... args);

    CardProtocol::PeerAcceptState acceptPeer(
        const std::string& identity,
        const CardProtocol::PositionVector& positions) override;
    void handleNotify(const BridgeEngine::CallMade&) override;
    void handleNotify(const BridgeEngine::CardPlayed&) override;
    void handleNotify(const BridgeEngine::DummyRevealed&) override;
    void handleNotify(const BridgeEngine::DealEnded&) override;
    void handleNotify(const CardManager::ShufflingState& state) override;

    const Player* getPlayerFor(
        const std::string& identity, const boost::optional<Position>& position);

    Reply<Position> hello(const std::string& indentity);
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
    SimpleCardProtocol cardProtocol {peerCommandSender};
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
            CardProtocol::PositionVector(positions.begin(), positions.end())));
}

void BridgeMain::Impl::run()
{
    messageLoop.run();
}

void BridgeMain::Impl::terminate()
{
    messageLoop.terminate();
}

bool BridgeMain::Impl::readyToStart() const
{
    return peerClientControl.arePlayersControlled(
        boost::make_indirect_iterator(secondIterator(players.begin())),
        boost::make_indirect_iterator(secondIterator(players.end())));
}

void BridgeMain::Impl::startIfReady()
{
    if (readyToStart()) {
        engine.initiate();
    }
}

CardProtocol& BridgeMain::Impl::getCardProtocol()
{
    return cardProtocol;
}

BridgeEngine& BridgeMain::Impl::getEngine()
{
    return engine;
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
    publish(
        CALL_COMMAND,
        std::make_pair(POSITION_COMMAND, position),
        std::make_pair(CALL_COMMAND, std::cref(event.call)));
    sendToPeersIfSelfControlledPlayer(
        event.player,
        CALL_COMMAND,
        std::make_pair(POSITION_COMMAND, position),
        std::make_pair(CALL_COMMAND, std::cref(event.call)));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::CardPlayed& event)
{
    const auto player_position = engine.getPosition(event.player);
    const auto hand_position = engine.getPosition(event.hand);
    const auto card_type = event.card.getType().get();
    publish(
        PLAY_COMMAND,
        std::make_pair(POSITION_COMMAND, hand_position),
        std::make_pair(CARD_COMMAND, std::cref(card_type)));
    sendToPeersIfSelfControlledPlayer(
        event.player,
        PLAY_COMMAND,
        std::make_pair(POSITION_COMMAND, player_position),
        std::make_pair(CARD_COMMAND, std::cref(card_type)));
}

void BridgeMain::Impl::handleNotify(const BridgeEngine::DummyRevealed&)
{
    publish(DUMMY_COMMAND);
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

CardProtocol::PeerAcceptState BridgeMain::Impl::acceptPeer(
    const std::string& identity, const CardProtocol::PositionVector& positions)
{
    const auto success = peerClientControl.addPeer(
        identity,
        positionPlayerIterator(positions.begin()),
        positionPlayerIterator(positions.end()));
    if (success) {
        if (readyToStart()) {
            // Engine initialization is deferred to give the card protocol
            // opportunity to initialize before cards are requested
            messageLoop.callOnce(
                [&engine = this->engine]() { engine.initiate(); });
            return CardProtocol::PeerAcceptState::ALL_ACCEPTED;
        }
        return CardProtocol::PeerAcceptState::ACCEPTED;
    }
    return CardProtocol::PeerAcceptState::REJECTED;
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
    auto& card_protocol = impl->getCardProtocol();
    card_protocol.setAcceptor(impl);
    auto& engine = impl->getEngine();
    engine.subscribeToCallMade(impl);
    engine.subscribeToCardPlayed(impl);
    engine.subscribeToDummyRevealed(impl);
    engine.subscribeToDealEnded(impl);
    dereference(card_protocol.getCardManager()).subscribe(impl);
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
