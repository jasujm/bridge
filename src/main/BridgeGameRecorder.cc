#include "main/BridgeGameRecorder.hh"

#include "bridge/Bidding.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/Deal.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "bridge/SimpleCard.hh"
#include "bridge/Trick.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "engine/SimpleCardManager.hh"
#include "Utility.hh"

#include <boost/range/adaptors.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <rocksdb/db.h>

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace Bridge {
namespace Main {

namespace {

const auto DB_ERROR_MSG = std::string {"Failed to open database: "};

enum class RecordType : char
{
    GAME = 0,
    DEAL = 16,
    BIDDING = 17,
    TRICKS = 18,
    PLAYER = 32,
};

auto recordKey(const Uuid& dealUuid, RecordType type)
{
    auto ret = std::string(sizeof(Uuid) + sizeof(RecordType), '\0');
    std::memmove(ret.data(), &dealUuid, sizeof(Uuid));
    std::memmove(ret.data() + sizeof(Uuid), &type, sizeof(RecordType));
    return ret;
}

auto makeDb(const std::string& path)
{
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, path, &db);
    if (!status.ok()) {
        throw std::runtime_error {DB_ERROR_MSG + status.ToString()};
    }
    return std::unique_ptr<rocksdb::DB> {db};
}

struct DealRecord {
    CardType cards[N_CARDS];
    Position openingPosition;
    Vulnerability vulnerability;
};

struct TrickRecord {
    Position leaderPosition;
    std::optional<CardType> cards[Trick::N_CARDS_IN_TRICK];
};

////////////////////////////////////////////////////////////////////////////////
// RecordedBidding
////////////////////////////////////////////////////////////////////////////////

class RecordedBidding : public Bidding {
public:

    RecordedBidding(const Position openingPosition, std::vector<Call> calls);

private:

    void handleAddCall(const Call&) override;
    int handleGetNumberOfCalls() const override;
    Position handleGetOpeningPosition() const override;
    Call handleGetCall(int n) const override;

    const Position openingPosition;
    const std::vector<Call> calls;
};

RecordedBidding::RecordedBidding(
    const Position openingPosition, std::vector<Call> calls) :
    openingPosition {openingPosition}, calls {std::move(calls)}
{
}

void RecordedBidding::handleAddCall(const Call&)
{
    throw "not implemented";
}

int RecordedBidding::handleGetNumberOfCalls() const
{
    return ssize(calls);
}

Position RecordedBidding::handleGetOpeningPosition() const
{
    return openingPosition;
}

Call RecordedBidding::handleGetCall(const int n) const
{
    return calls.at(n);
}

////////////////////////////////////////////////////////////////////////////////
// RecordedHand
////////////////////////////////////////////////////////////////////////////////

class RecordedHand : public Hand {
public:

    RecordedHand(const SimpleCard* cards);

private:

    void handleSubscribe(std::weak_ptr<CardRevealStateObserver>) override;
    void handleRequestReveal(const IndexVector&) override;
    void handleMarkPlayed(int) override;
    const Card& handleGetCard(int n) const override;
    bool handleIsPlayed(int) const override;
    int handleGetNumberOfCards() const override;

    // TODO: Replace with span in C++20
    const SimpleCard* const cards;
};

RecordedHand::RecordedHand(const SimpleCard* cards) :
    cards {cards}
{
}

void RecordedHand::handleSubscribe(std::weak_ptr<CardRevealStateObserver>)
{
    throw "not implemented";
}

void RecordedHand::handleRequestReveal(const IndexVector&)
{
    throw "not implemented";
}

void RecordedHand::handleMarkPlayed(int)
{
    throw "not implemented";
}

const Card& RecordedHand::handleGetCard(int n) const
{
    assert(0 <= n && n < N_CARDS_PER_PLAYER);
    return cards[n];
}

bool RecordedHand::handleIsPlayed(int) const
{
    // TODO: Implement this
    return false;
}

int RecordedHand::handleGetNumberOfCards() const
{
    return N_CARDS_PER_PLAYER;
}

////////////////////////////////////////////////////////////////////////////////
// RecordedTrick
////////////////////////////////////////////////////////////////////////////////

class RecordedTrick : public Trick {
public:

    RecordedTrick(
        const std::vector<SimpleCard>& cards,
        const std::array<RecordedHand, N_PLAYERS>& hands,
        const TrickRecord& record);

private:

    auto initializeCards(
        const std::vector<SimpleCard>& cards, const TrickRecord& record);

    void handleAddCardToTrick(const Card&) override;
    int handleGetNumberOfCardsPlayed() const override;
    const Card& handleGetCard(int n) const override;
    const Hand& handleGetHand(int n) const override;

    const Position leaderPosition;
    // TODO: Replace with span in C++20
    const std::array<const Card*, N_CARDS_IN_TRICK> cards;
    const std::array<RecordedHand, N_PLAYERS>& hands;
};

auto RecordedTrick::initializeCards(
    const std::vector<SimpleCard>& cards, const TrickRecord& record)
{
    std::array<const Card*, N_CARDS_IN_TRICK> ret {};
    for (const auto n : to(ret.size())) {
        if (const auto& ct = record.cards[n]) {
            const auto iter = std::find_if(
                cards.begin(), cards.end(),
                [&ct](const auto& c) { return *ct == c.getType(); });
            assert(iter != cards.end());
            ret[n] = std::addressof(*iter);
        }

    }
    return ret;
}

RecordedTrick::RecordedTrick(
    const std::vector<SimpleCard>& cards,
    const std::array<RecordedHand, N_PLAYERS>& hands,
    const TrickRecord& record) :
    leaderPosition {record.leaderPosition},
    cards {initializeCards(cards, record)},
    hands {hands}
{
}

void RecordedTrick::handleAddCardToTrick(const Card&)
{
    throw "not implemented";
}
int RecordedTrick::handleGetNumberOfCardsPlayed() const
{
    const auto first_unplayed_iter = std::find_if(
        cards.begin(), cards.end(),
        [](const auto& card) { return !card; });
    return first_unplayed_iter - cards.begin();
}
const Card& RecordedTrick::handleGetCard(const int n) const
{
    assert(0 <= n && n < N_CARDS_IN_TRICK);
    assert(cards[n]);
    return *cards[n];
}

const Hand& RecordedTrick::handleGetHand(const int n) const
{
    const auto m = positionOrder(clockwise(leaderPosition, n));
    assert(0 <= m && m < N_PLAYERS);
    return hands[m];
}

////////////////////////////////////////////////////////////////////////////////
// RecordedDeal
////////////////////////////////////////////////////////////////////////////////

class RecordedDeal : public Deal {
public:

    RecordedDeal(
        const Uuid& uuid, const DealRecord& dealRecord,
        std::vector<Call> calls, std::vector<TrickRecord> tricks);

private:

    auto initializeHand(Position position);
    auto trickIterator(std::vector<TrickRecord>::const_iterator iter);

    DealPhase handleGetPhase() const override;
    const Uuid& handleGetUuid() const override;
    Vulnerability handleGetVulnerability() const override;
    const Hand& handleGetHand(Position position) const override;
    const Card& handleGetCard(int n) const override;
    const Bidding& handleGetBidding() const override;
    int handleGetNumberOfTricks() const override;
    const Trick& handleGetTrick(int n) const override;

    const Uuid uuid;
    const std::vector<SimpleCard> cards;
    const std::array<RecordedHand, N_PLAYERS> hands;
    const Vulnerability vulnerability;
    const RecordedBidding bidding;
    const std::vector<RecordedTrick> tricks;
};

auto RecordedDeal::initializeHand(const Position position)
{
    const auto n = positionOrder(position) * N_CARDS_PER_PLAYER;
    return RecordedHand {&cards[n]};
}

auto RecordedDeal::trickIterator(std::vector<TrickRecord>::const_iterator iter)
{
    const auto func = [this](const auto& record)
    {
        return RecordedTrick {this->cards, this->hands, record};
    };
    return boost::make_transform_iterator(iter, func);
}

RecordedDeal::RecordedDeal(
    const Uuid& uuid, const DealRecord& dealRecord,
    std::vector<Call> calls, std::vector<TrickRecord> tricks) :
    uuid {uuid},
    cards(std::begin(dealRecord.cards), std::end(dealRecord.cards)),
    hands {
        initializeHand(Positions::NORTH),
        initializeHand(Positions::EAST),
        initializeHand(Positions::SOUTH),
        initializeHand(Positions::WEST),
    },
    vulnerability {dealRecord.vulnerability},
    bidding {dealRecord.openingPosition, std::move(calls)},
    tricks(trickIterator(tricks.begin()), trickIterator(tricks.end()))
{
}

DealPhase RecordedDeal::handleGetPhase() const
{
    const auto n_tricks = ssize(tricks);
    if (n_tricks == 0) {
        return DealPhase::BIDDING;
    } else if (tricks[n_tricks - 1].isCompleted()) {
        return DealPhase::ENDED;
    }
    return DealPhase::BIDDING;

}

const Uuid& RecordedDeal::handleGetUuid() const
{
    return uuid;
}

Vulnerability RecordedDeal::handleGetVulnerability() const
{
    return vulnerability;
}

const Hand& RecordedDeal::handleGetHand(const Position position) const
{
    const auto n = positionOrder(position);
    assert(0 <= n && n < ssize(hands));
    return hands[n];
}

const Card& RecordedDeal::handleGetCard(const int n) const
{
    assert(0 <= n && n < ssize(cards));
    return cards[n];
}

const Bidding& RecordedDeal::handleGetBidding() const
{
    return bidding;
}

int RecordedDeal::handleGetNumberOfTricks() const
{
    return ssize(tricks);
}

const Trick& RecordedDeal::handleGetTrick(const int n) const
{
    assert(0 <= n && n < ssize(tricks));
    return tricks[n];
}

}

////////////////////////////////////////////////////////////////////////////////
// BridgeGameRecorder::Impl
////////////////////////////////////////////////////////////////////////////////

class BridgeGameRecorder::Impl {
public:
    Impl(const std::string_view path);

    template<typename... Args>
    void put(Args&&... args);

    template<typename... Args>
    auto get(Args&&... args);

private:

    std::unique_ptr<rocksdb::DB> db;
};

BridgeGameRecorder::Impl::Impl(const std::string_view path) :
    db {makeDb(std::string {path})}
{
}

template<typename... Args>
void BridgeGameRecorder::Impl::put(Args&&... args)
{
    assert(db);
    db->Put(rocksdb::WriteOptions(), std::forward<Args>(args)...);
}

template<typename... Args>
auto BridgeGameRecorder::Impl::get(Args&&... args)
{
    assert(db);
    return db->Get(rocksdb::ReadOptions(), std::forward<Args>(args)...);
}

BridgeGameRecorder::BridgeGameRecorder(const std::string_view path) :
    impl {std::make_unique<Impl>(path)}
{
}

BridgeGameRecorder::~BridgeGameRecorder() = default;

void BridgeGameRecorder::recordGame(
    const Uuid& gameUuid, const GameState& gameState)
{
    assert(impl);
    impl->put(
        recordKey(gameUuid, RecordType::GAME),
        rocksdb::Slice {
            reinterpret_cast<const char*>(&gameState), sizeof(GameState)});
}

void BridgeGameRecorder::recordDeal(const Deal& deal)
{
    assert(impl);
    const auto& deal_uuid = deal.getUuid();
    const auto& bidding = deal.getBidding();

    auto deal_record = DealRecord {};
    for (const auto n : to(N_CARDS)) {
        deal_record.cards[n] = dereference(deal.getCard(n).getType());
    }
    deal_record.openingPosition = bidding.getOpeningPosition();
    deal_record.vulnerability = deal.getVulnerability();
    impl->put(
        recordKey(deal_uuid, RecordType::DEAL),
        rocksdb::Slice {
            reinterpret_cast<const char*>(&deal_record), sizeof(DealRecord)});

    const auto n_calls = bidding.getNumberOfCalls();
    auto calls_record = std::vector<Call>(n_calls, Call {});
    for (const auto n : to(n_calls)) {
        calls_record[n] = bidding.getCall(n);
    }
    impl->put(
        recordKey(deal_uuid, RecordType::BIDDING),
        rocksdb::Slice {
            reinterpret_cast<const char*>(calls_record.data()),
            sizeof(Call) * n_calls});

    const auto n_tricks = deal.getNumberOfTricks();
    auto tricks_record = std::vector<TrickRecord>(n_tricks, TrickRecord {});
    for (const auto n : to(n_tricks)) {
        const auto& trick = deal.getTrick(n);
        tricks_record[n].leaderPosition = dereference(
            deal.getPosition(trick.getLeader()));
        for (const auto m : to(trick.getNumberOfCardsPlayed())) {
            const auto& card = dereference(trick.getCard(m));
            tricks_record[n].cards[m] = dereference(card.getType());
        }
    }
    impl->put(
        recordKey(deal_uuid, RecordType::TRICKS),
        rocksdb::Slice {
            reinterpret_cast<const char*>(tricks_record.data()),
            sizeof(TrickRecord) * n_tricks});
}

void BridgeGameRecorder::recordPlayer(
    const Uuid& playerUuid, const Messaging::UserIdView userId)
{
    assert(impl);
    impl->put(
        recordKey(playerUuid, RecordType::PLAYER),
        rocksdb::Slice {userId.data(), userId.size()});
}

std::optional<BridgeGameRecorder::GameState>
BridgeGameRecorder::recallGame(const Uuid& gameUuid)
{
    assert(impl);
    auto value = std::string {};
    impl->get(recordKey(gameUuid, RecordType::GAME), &value);
    if (value.size() < sizeof(GameState)) {
        return std::nullopt;
    }
    static_assert( std::is_trivially_copyable_v<GameState> );
    auto game_state = GameState {};
    std::memmove(&game_state, value.data(), sizeof(GameState));
    return game_state;
}

std::optional<BridgeGameRecorder::DealState>
BridgeGameRecorder::recallDeal(const Uuid& dealUuid)
{
    assert(impl);
    auto serialized_record = std::string {};
    impl->get(recordKey(dealUuid, RecordType::DEAL), &serialized_record);
    if (serialized_record.size() < sizeof(DealRecord)) {
        return std::nullopt;
    }
    auto deal_record = DealRecord {};
    std::memmove(&deal_record, serialized_record.data(), sizeof(DealRecord));
    static_assert( std::is_trivially_copyable_v<DealRecord> );

    impl->get(recordKey(dealUuid, RecordType::BIDDING), &serialized_record);
    auto bidding_record = std::vector<Call>(
        serialized_record.size() / sizeof(Call), Call {});
    static_assert( std::is_trivially_copyable_v<Call> );
    std::memmove(
        bidding_record.data(), serialized_record.data(),
        serialized_record.size());

    impl->get(recordKey(dealUuid, RecordType::TRICKS), &serialized_record);
    auto tricks_record = std::vector<TrickRecord>(
        serialized_record.size() / sizeof(TrickRecord), TrickRecord {});
    static_assert( std::is_trivially_copyable_v<TrickRecord> );
    std::memmove(
        tricks_record.data(), serialized_record.data(),
        serialized_record.size());

    return DealState {
        std::make_unique<RecordedDeal>(
            dealUuid, deal_record, std::move(bidding_record),
            std::move(tricks_record)),
        std::make_shared<Engine::SimpleCardManager>(
            std::begin(deal_record.cards), std::end(deal_record.cards)),
        std::make_shared<Engine::DuplicateGameManager>(
            deal_record.openingPosition, deal_record.vulnerability),
    };
}

std::optional<Messaging::UserId>
BridgeGameRecorder::recallPlayer(const Uuid& playerUuid)
{
    assert(impl);
    auto user_id = Messaging::UserId {};
    const auto status = impl->get(
        recordKey(playerUuid, RecordType::PLAYER), &user_id);
    return status.ok() ? std::optional {std::move(user_id)} : std::nullopt;
}

}
}
