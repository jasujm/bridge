// The structs used for writing records to the database only consist of members
// with alignment 1, so the assumption is that the compiler won't generate any
// padding for them. Just to be extra sure, for GCC we use packed attribute.

#if __GNUC__
#define BRIDGE_PACKED __attribute__((packed))
#else // __GNUC__
#define BRIDGE_PACKED
#endif // __GNUC__

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
#include "Logging.hh"
#include "Utility.hh"

#include <boost/iterator/transform_iterator.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/uuid/uuid_io.hpp>

#if WITH_RECORDER
#include <rocksdb/db.h>
#endif // WITH_RECORDER

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace Bridge {
namespace Main {

#if WITH_RECORDER

namespace {

const auto DB_ERROR_MSG = std::string {"Failed to open database: "};

constexpr auto RECORD_VERSION = 1;

enum class RecordType : std::uint8_t
{
    GAME = 0,
    DEAL = 16,
    BIDDING = 17,
    TRICKS = 18,
    PLAYER = 32,
};

enum class PositionRecord : std::uint8_t {};
enum class CardRecord : std::uint8_t {};
enum class OptionalCardRecord : std::uint8_t {};
enum class CallRecord : std::uint8_t {};
enum class DealConfigRecord : std::uint8_t {};
using Version = std::uint8_t;

struct GameStateRecord {
    std::uint8_t flags;
    Uuid playerUuids[4];
    Uuid dealUuid;
} BRIDGE_PACKED;

struct GameRecord {
    Version version;
    GameStateRecord state;
} BRIDGE_PACKED;

struct DealRecord {
    Version version;
    CardRecord cards[N_CARDS];
    DealConfigRecord config;
} BRIDGE_PACKED;

struct TrickRecord {
    PositionRecord leaderPosition;
    OptionalCardRecord cards[Trick::N_CARDS_IN_TRICK];
} BRIDGE_PACKED;

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

GameStateRecord packGameState(const BridgeGameRecorder::GameState& state)
{
    auto ret = GameStateRecord {};
    for (const auto n : to(N_PLAYERS)) {
        if (const auto& u = state.playerUuids[n]) {
            ret.flags |= (1 << n);
            ret.playerUuids[n] = *u;
        }
    }
    if (const auto& u = state.dealUuid) {
        ret.flags |= 0x10;
        ret.dealUuid = *u;
    }
    return ret;
}

BridgeGameRecorder::GameState unpackGameState(const GameStateRecord& state)
{
    auto ret = BridgeGameRecorder::GameState {};
    for (const auto n : to(N_PLAYERS)) {
        if (state.flags & (1 << n)) {
            ret.playerUuids[n] = state.playerUuids[n];
        }
    }
    if (state.flags & 0x10) {
        ret.dealUuid = state.dealUuid;
    }
    return ret;
}

CardRecord packCard(const CardType& card)
{
    const auto packed_rank = static_cast<std::uint8_t>(card.rank.get());
    const auto packed_suit = static_cast<std::uint8_t>(card.suit.get());
    return static_cast<CardRecord>(packed_rank << 3 | packed_suit);
}

CardType unpackCard(CardRecord card)
{
    const auto card_ = static_cast<std::uint8_t>(card);
    const auto packed_rank = (card_ & 0xf8) >> 3;
    const auto packed_suit = card_ & 0x07;
    return CardType {
        static_cast<RankLabel>(packed_rank),
        static_cast<SuitLabel>(packed_suit)};
}

template<typename Iterator>
auto unpackCardIterator(Iterator iter)
{
    return boost::make_transform_iterator(iter, &unpackCard);
}

OptionalCardRecord packOptionalCard(const std::optional<CardType>& card)
{
    if (card) {
        const auto packed_card = static_cast<std::uint8_t>(packCard(*card));
        return static_cast<OptionalCardRecord>(0x80 | packed_card);
    }
    return OptionalCardRecord {0};
}

std::optional<CardType> unpackOptionalCard(const OptionalCardRecord card)
{
    const auto card_ = static_cast<std::uint8_t>(card);
    if (card_ & 0x80) {
        return unpackCard(static_cast<CardRecord>(card_ & 0x7f));
    }
    return std::nullopt;
}

PositionRecord packPosition(const Position position)
{
    return static_cast<PositionRecord>(position.get());
}

Position unpackPosition(const PositionRecord position)
{
    return static_cast<PositionLabel>(position);
}

struct PackCallVisitor {
    CallRecord operator()(Pass) { return CallRecord {0x80}; }
    CallRecord operator()(Double) { return CallRecord {0x81}; }
    CallRecord operator()(Redouble) { return CallRecord {0x82}; }
    CallRecord operator()(const Bid& bid)
    {
        const auto packed_level = static_cast<std::uint8_t>(bid.level);
        const auto packed_strain = static_cast<std::uint8_t>(bid.strain.get());
        return static_cast<CallRecord>(packed_level << 4 | packed_strain);
    }
};

CallRecord packCall(const Call& call)
{
    return std::visit(PackCallVisitor {}, call);
}

Call unpackCall(const CallRecord call)
{
    const auto call_ = static_cast<std::uint8_t>(call);
    if (call_ & 0x80) {
        switch (call_ & 0x7f) {
        case 0:
            return Pass {};
        case 1:
            return Double {};
        case 2:
            return Redouble {};
        }
        throw std::runtime_error {"Unexpected call record"};
    } else {
        const auto packed_level = (call_ & 0xf0) >> 4;
        const auto packed_strain = call_ & 0x0f;
        return Bid {packed_level, static_cast<StrainLabel>(packed_strain)};
    }
}

template<typename Iterator>
auto unpackCallIterator(Iterator iter)
{
    return boost::make_transform_iterator(iter, &unpackCall);
}

DealConfigRecord packDealConfig(
    const Position openingPosition, const Vulnerability& vulnerability)
{
    const auto packed_position = static_cast<std::uint8_t>(
        packPosition(openingPosition));
    const auto packed_ns_vulnerable = static_cast<std::uint8_t>(
        vulnerability.northSouthVulnerable);
    const auto packed_ew_vulnerable = static_cast<std::uint8_t>(
        vulnerability.eastWestVulnerable);
    return static_cast<DealConfigRecord>(
        packed_position | packed_ns_vulnerable << 2 |
        packed_ew_vulnerable << 3);
}

std::pair<Position, Vulnerability> unpackDealConfig(
    const DealConfigRecord config)
{
    const auto config_ = static_cast<std::uint8_t>(config);
    const auto packed_position = config_ & 0x03;
    const auto packed_ns_vulnerable = config_ & 0x04;
    const auto packed_ew_vulnerable = config_ & 0x08;
    return {
        unpackPosition(static_cast<PositionRecord>(packed_position)),
        {
            static_cast<bool>(packed_ns_vulnerable),
            static_cast<bool>(packed_ew_vulnerable)
        }
    };
}

////////////////////////////////////////////////////////////////////////////////
// RecordedBidding
////////////////////////////////////////////////////////////////////////////////

class RecordedBidding : public Bidding {
public:

    RecordedBidding(
        Position openingPosition, const std::vector<CallRecord>& calls);

private:

    void handleAddCall(const Call&) override;
    int handleGetNumberOfCalls() const override;
    Position handleGetOpeningPosition() const override;
    Call handleGetCall(int n) const override;

    const Position openingPosition;
    const std::vector<Call> calls;
};

RecordedBidding::RecordedBidding(
    const Position openingPosition, const std::vector<CallRecord>& calls) :
    openingPosition {openingPosition},
    calls(unpackCallIterator(calls.begin()), unpackCallIterator(calls.end()))
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
    assert(0 <= n && n < ssize(calls));
    return calls[n];
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
        if (const auto& ct = unpackOptionalCard(record.cards[n])) {
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
    leaderPosition {unpackPosition(record.leaderPosition)},
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
        const std::vector<CallRecord>& calls,
        const std::vector<TrickRecord>& tricks);

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
    const std::vector<CallRecord>& calls,
    const std::vector<TrickRecord>& tricks) :
    uuid {uuid},
    cards(
        unpackCardIterator(std::begin(dealRecord.cards)),
        unpackCardIterator(std::end(dealRecord.cards))),
    hands {
        initializeHand(Positions::NORTH),
        initializeHand(Positions::EAST),
        initializeHand(Positions::SOUTH),
        initializeHand(Positions::WEST),
    },
    vulnerability {unpackDealConfig(dealRecord.config).second},
    bidding {unpackDealConfig(dealRecord.config).first, calls},
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

#else // WITH_RECORDER

struct BridgeGameRecorder::Impl {
    Impl(std::string_view) {}
};

#endif // WITH_RECORDER

BridgeGameRecorder::BridgeGameRecorder(const std::string_view path) :
    impl {std::make_unique<Impl>(path)}
{
}

BridgeGameRecorder::~BridgeGameRecorder() = default;

void BridgeGameRecorder::recordGame(
    [[maybe_unused]] const Uuid& gameUuid,
    [[maybe_unused]] const GameState& gameState)
{
#if WITH_RECORDER
    assert(impl);
    auto game_record = GameRecord {};
    game_record.version = RECORD_VERSION;
    game_record.state = packGameState(gameState);
    impl->put(
        recordKey(gameUuid, RecordType::GAME),
        rocksdb::Slice {
            reinterpret_cast<const char*>(&game_record), sizeof(GameRecord)});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordDeal([[maybe_unused]] const Deal& deal)
{
#if WITH_RECORDER
    assert(impl);
    const auto& deal_uuid = deal.getUuid();
    const auto& bidding = deal.getBidding();

    auto deal_record = DealRecord {};
    deal_record.version = RECORD_VERSION;
    for (const auto n : to(N_CARDS)) {
        deal_record.cards[n] = packCard(dereference(deal.getCard(n).getType()));
    }
    deal_record.config = packDealConfig(
        bidding.getOpeningPosition(), deal.getVulnerability());
    impl->put(
        recordKey(deal_uuid, RecordType::DEAL),
        rocksdb::Slice {
            reinterpret_cast<const char*>(&deal_record), sizeof(DealRecord)});

    const auto n_calls = bidding.getNumberOfCalls();
    auto calls_record = std::vector<CallRecord>(n_calls, CallRecord {});
    for (const auto n : to(n_calls)) {
        calls_record[n] = packCall(bidding.getCall(n));
    }
    impl->put(
        recordKey(deal_uuid, RecordType::BIDDING),
        rocksdb::Slice {
            reinterpret_cast<const char*>(calls_record.data()),
            static_cast<std::size_t>(n_calls)});

    const auto n_tricks = deal.getNumberOfTricks();
    auto tricks_record = std::vector<TrickRecord>(n_tricks, TrickRecord {});
    for (const auto n : to(n_tricks)) {
        const auto& trick = deal.getTrick(n);
        tricks_record[n].leaderPosition = packPosition(
            dereference(deal.getPosition(trick.getLeader())));
        for (const auto m : to(trick.getNumberOfCardsPlayed())) {
            const auto& card = dereference(trick.getCard(m));
            tricks_record[n].cards[m] = packOptionalCard(card.getType());
        }
    }
    impl->put(
        recordKey(deal_uuid, RecordType::TRICKS),
        rocksdb::Slice {
            reinterpret_cast<const char*>(tricks_record.data()),
            sizeof(TrickRecord) * n_tricks});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordPlayer(
    [[maybe_unused]] const Uuid& playerUuid,
    [[maybe_unused]] const Messaging::UserIdView userId)
{
#if WITH_RECORDER
    assert(impl);
    auto serialized_record = static_cast<char>(RECORD_VERSION) + std::string {userId};
    impl->put(recordKey(playerUuid, RecordType::PLAYER), serialized_record);
#endif // WITH_RECORDER
}

std::optional<BridgeGameRecorder::GameState>
BridgeGameRecorder::recallGame([[maybe_unused]] const Uuid& gameUuid)
{
#if WITH_RECORDER
    assert(impl);
    auto serialized_record = std::string {};
    impl->get(recordKey(gameUuid, RecordType::GAME), &serialized_record);
    if (serialized_record.size() < sizeof(GameRecord)) {
        log(LogLevel::WARNING,
            "Unexpected record size while recalling game %s", gameUuid);
        return std::nullopt;
    }
    static_assert( std::is_trivially_copyable_v<GameRecord> );
    auto game_record = GameRecord {};
    std::memmove(&game_record, serialized_record.data(), sizeof(GameRecord));
    if (game_record.version != RECORD_VERSION) {
        log(LogLevel::WARNING,
            "Unexpected record version %d while recalling game %s",
            game_record.version, gameUuid);
        return std::nullopt;
    }
    return unpackGameState(game_record.state);
#else // WITH_RECORDER
    return std::nullopt;
#endif // WITH_RECORDER
}

std::optional<BridgeGameRecorder::DealState>
BridgeGameRecorder::recallDeal([[maybe_unused]] const Uuid& dealUuid)
{
#if WITH_RECORDER
    assert(impl);
    auto serialized_record = std::string {};
    impl->get(recordKey(dealUuid, RecordType::DEAL), &serialized_record);
    if (serialized_record.size() < sizeof(DealRecord)) {
        log(LogLevel::WARNING,
            "Unexpected record size while recalling deal %s", dealUuid);
        return std::nullopt;
    }
    auto deal_record = DealRecord {};
    static_assert( std::is_trivially_copyable_v<DealRecord> );
    std::memmove(&deal_record, serialized_record.data(), sizeof(DealRecord));
    if (deal_record.version != RECORD_VERSION) {
        log(LogLevel::WARNING,
            "Unexpected record version %d while recalling deal %s",
            deal_record.version, dealUuid);
        return std::nullopt;
    }

    impl->get(recordKey(dealUuid, RecordType::BIDDING), &serialized_record);
    auto calls_record = std::vector<CallRecord>(
        serialized_record.size() / sizeof(CallRecord), CallRecord {});
    static_assert( std::is_trivially_copyable_v<CallRecord> );
    std::memmove(
        calls_record.data(), serialized_record.data(),
        serialized_record.size());

    impl->get(recordKey(dealUuid, RecordType::TRICKS), &serialized_record);
    auto tricks_record = std::vector<TrickRecord>(
        serialized_record.size() / sizeof(TrickRecord), TrickRecord {});
    static_assert( std::is_trivially_copyable_v<TrickRecord> );
    std::memmove(
        tricks_record.data(), serialized_record.data(),
        serialized_record.size());

    const auto [openingPosition, vulnerability] = unpackDealConfig(
        deal_record.config);
    return DealState {
        std::make_unique<RecordedDeal>(
            dealUuid, deal_record, std::move(calls_record),
            std::move(tricks_record)),
        std::make_shared<Engine::SimpleCardManager>(
            unpackCardIterator(std::begin(deal_record.cards)),
            unpackCardIterator(std::end(deal_record.cards))),
        std::make_shared<Engine::DuplicateGameManager>(
            openingPosition, vulnerability)
    };
#else // WITH_RECORDER
    return std::nullopt;
#endif // WITH_RECORDER
}

std::optional<Messaging::UserId>
BridgeGameRecorder::recallPlayer([[maybe_unused]] const Uuid& playerUuid)
{
#if WITH_RECORDER
    assert(impl);
    auto serialized_record = std::string {};
    impl->get(recordKey(playerUuid, RecordType::PLAYER), &serialized_record);
    if (serialized_record.size() < sizeof(Version)) {
        log(LogLevel::WARNING,
            "Unexpected record size while recalling player %s", playerUuid);
        return std::nullopt;
    }
    const auto version = static_cast<Version>(serialized_record[0]);
    if (version != RECORD_VERSION) {
        log(LogLevel::WARNING,
            "Unexpected record version %d while recalling player %s",
            version, playerUuid);
        return std::nullopt;
    }
    return Messaging::UserId(
        serialized_record.data() + sizeof(Version),
        serialized_record.size() - sizeof(Version));
#else // WITH_RECORDER
    return std::nullopt;
#endif // WITH_RECORDER
}

}
}
