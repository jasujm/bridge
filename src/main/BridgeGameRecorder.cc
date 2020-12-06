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
#include "bridge/CallIterator.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Deal.hh"
#include "bridge/Hand.hh"
#include "bridge/Position.hh"
#include "bridge/SimpleCard.hh"
#include "bridge/Trick.hh"
#include "bridge/Vulnerability.hh"
#include "engine/DuplicateGameManager.hh"
#include "main/PeerlessCardProtocol.hh"
#include "Enumerate.hh"
#include "Logging.hh"
#include "Utility.hh"

#include <boost/endian/arithmetic.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/uuid/uuid_io.hpp>

#if WITH_RECORDER
#include <rocksdb/db.h>
#include <rocksdb/merge_operator.h>
#endif // WITH_RECORDER

#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace Bridge {
namespace Main {

#if WITH_RECORDER

namespace {

enum class RecordType {
    DEFAULT,
    GAME,
    DEAL,
    BIDDING,
    TRICKS,
    PLAYER,
    DEAL_RESULT,
};

const auto COLUMN_FAMILY_NAMES = std::array {
    // Default missing here. Otherwise must match the enumerators above.
    std::string {"game_v1"},
    std::string {"deal_v1"},
    std::string {"bidding_v1"},
    std::string {"tricks_v1"},
    std::string {"player_v1"},
    std::string {"deal_result_v1"},
};

const auto DB_ERROR_MSG = std::string {"Failed to open database: "};

class BridgeRecorderMergeOperator : public rocksdb::AssociativeMergeOperator {
public:
    bool Merge(
        const rocksdb::Slice& key, const rocksdb::Slice* existingValue,
        const rocksdb::Slice& value, std::string* newValue,
        rocksdb::Logger* logger) const override;
    const char* Name() const override;
};

bool BridgeRecorderMergeOperator::Merge(
    const rocksdb::Slice&, const rocksdb::Slice* existingValue,
    const rocksdb::Slice& value, std::string* newValue, rocksdb::Logger*) const
{
    auto& new_value = dereference(newValue);
    if (existingValue) {
        new_value.assign(existingValue->data(), existingValue->size());
        new_value.append(value.data(), value.size());
    } else {
        new_value.assign(value.data(), value.size());
    }
    return true;
}

const char* BridgeRecorderMergeOperator::Name() const
{
    return "BridgeRecorderOperator";
}

enum class PositionRecord : std::uint8_t {};
enum class CardRecord : std::uint8_t {};
enum class CallRecord : std::uint8_t {};
enum class DealConfigRecord : std::uint8_t {};
using PackedScore = boost::endian::big_int16_t;

struct GameRecord {
    Uuid playerUuids[4];
    Uuid dealUuid;
} BRIDGE_PACKED;

struct DealRecord {
    CardRecord cards[N_CARDS];
    DealConfigRecord config;
} BRIDGE_PACKED;

struct TrickRecord {
    PositionRecord leaderPosition;
    CardRecord cards[Trick::N_CARDS_IN_TRICK];
} BRIDGE_PACKED;

struct DealResultRecord {
    Uuid dealUuid;
    PackedScore northSouthScore;
};

rocksdb::Slice uuidKey(const Uuid& uuid)
{
    return {reinterpret_cast<const char*>(&uuid), sizeof(Uuid)};
}

CardRecord packCard(const CardType& card)
{
    return static_cast<CardRecord>(cardTypeIndex(card));
}

CardType unpackCard(CardRecord card)
{
    return enumerateCardType(static_cast<int>(card));
}

template<typename Iterator>
auto unpackCardIterator(Iterator iter)
{
    return boost::make_transform_iterator(iter, &unpackCard);
}

PositionRecord packPosition(const Position position)
{
    return static_cast<PositionRecord>(position.get());
}

Position unpackPosition(const PositionRecord position)
{
    return static_cast<PositionLabel>(position);
}

CallRecord packCall(const Call& call)
{
    return static_cast<CallRecord>(callIndex(call));
}

Call unpackCall(const CallRecord call)
{
    return enumerateCall(static_cast<int>(call));
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

template<typename Record>
auto getMultipleRecords(const std::string& serializedRecords)
{
    static_assert( std::is_trivially_copyable_v<Record> );
    // The serialized record may contain partial record as the last element. The
    // members not initialized from the record are left zero initialized.
    const auto record_size_rounded_up =
        (serializedRecords.size() + sizeof(Record) - 1);
    const auto n_records = record_size_rounded_up / sizeof(Record);
    const auto last_record_size = record_size_rounded_up % sizeof(Record) + 1;
    auto records = std::vector<Record>(n_records, Record {});
    std::memmove(
        records.data(), serializedRecords.data(), serializedRecords.size());
    return std::tuple {records, last_record_size};
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
        const TrickRecord& record,
        int nCardsInTrick);

private:

    auto initializeCards(
        const std::vector<SimpleCard>& cards, const TrickRecord& record,
        int nCardsInTrick);

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
    const std::vector<SimpleCard>& cards, const TrickRecord& record,
    const int nCardsInTrick)
{
    std::array<const Card*, N_CARDS_IN_TRICK> ret {};
    for (const auto n : to(nCardsInTrick)) {
        const auto ct = unpackCard(record.cards[n]);
        const auto iter = std::find_if(
            cards.begin(), cards.end(),
            [&ct](const auto& c) { return ct == c.getType(); });
        assert(iter != cards.end());
        ret[n] = std::addressof(*iter);
    }
    return ret;
}

RecordedTrick::RecordedTrick(
    const std::vector<SimpleCard>& cards,
    const std::array<RecordedHand, N_PLAYERS>& hands,
    const TrickRecord& record,
    const int nCardsInTrick) :
    leaderPosition {unpackPosition(record.leaderPosition)},
    cards {initializeCards(cards, record, nCardsInTrick)},
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
        const std::vector<TrickRecord>& tricks,
        int nCardsInLastTrick);

private:

    auto initializeHand(Position position);
    auto initializeTricks(
        const std::vector<TrickRecord>& tricks, int nCardsInLastTrick);

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

auto RecordedDeal::initializeTricks(
    const std::vector<TrickRecord>& tricks, const int nCardsInLastTrick)
{
    auto ret = std::vector<RecordedTrick> {};
    ret.reserve(tricks.size());
    for (const auto& [n, record] : enumerate(tricks)) {
        const auto n_cards_in_trick =
            (n + 1 == ssize(tricks)) ?
            nCardsInLastTrick : Trick::N_CARDS_IN_TRICK;
        ret.emplace_back(this->cards, this->hands, record, n_cards_in_trick);
    }
    return ret;
}

RecordedDeal::RecordedDeal(
    const Uuid& uuid, const DealRecord& dealRecord,
    const std::vector<CallRecord>& calls,
    const std::vector<TrickRecord>& tricks,
    const int nCardsInLastTrick) :
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
    tricks {initializeTricks(tricks, nCardsInLastTrick)}
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
    Impl(const std::string& path);
    ~Impl();

    template<typename... Args>
    void put(RecordType recordType, const Uuid& uuid, Args&&... args);

    template<typename... Args>
    [[nodiscard]] auto get(
        RecordType recordType, const Uuid& uuid, Args&&... args);

    template<typename... Args>
    void merge(RecordType recordType, const Uuid& uuid, Args&&... args);

private:

    rocksdb::ColumnFamilyHandle*
    internalGetColumnFamilyHandle(RecordType recordType);

    std::unique_ptr<rocksdb::DB> db;
    std::vector<rocksdb::ColumnFamilyHandle*> columnFamilyHandles;
};

BridgeGameRecorder::Impl::Impl(const std::string& path)
{
    auto db = static_cast<rocksdb::DB*>(nullptr);
    auto options = rocksdb::Options {};
    options.create_if_missing = true;
    options.create_missing_column_families = true;
    options.merge_operator = std::make_shared<BridgeRecorderMergeOperator>();
    auto column_families = std::vector<rocksdb::ColumnFamilyDescriptor> {};
    column_families.emplace_back(); // this is the default column family
    for (const auto& name : COLUMN_FAMILY_NAMES) {
        column_families.emplace_back(name, options);
    }
    const auto status = rocksdb::DB::Open(
        options, path, column_families, &columnFamilyHandles, &db);
    if (!status.ok()) {
        throw std::runtime_error {DB_ERROR_MSG + status.ToString()};
    }
    this->db.reset(db);
}

BridgeGameRecorder::Impl::~Impl()
{
    assert(db);
    for (auto handle : columnFamilyHandles) {
        const auto status = db->DestroyColumnFamilyHandle(handle);
        assert(status.ok());
    }
}

template<typename... Args>
void BridgeGameRecorder::Impl::put(
    const RecordType recordType, const Uuid& uuid, Args&&... args)
{
    assert(db);
    const auto handle = internalGetColumnFamilyHandle(recordType);
    const auto key = uuidKey(uuid);
    const auto status = db->Put(
        rocksdb::WriteOptions {}, handle, key, std::forward<Args>(args)...);
    if (!status.ok()) {
        log(LogLevel::WARNING,
            "Failed database put operation: %s", status.ToString());
    }
}

template<typename... Args>
auto BridgeGameRecorder::Impl::get(
    RecordType recordType, const Uuid& uuid, Args&&... args)
{
    assert(db);
    const auto handle = internalGetColumnFamilyHandle(recordType);
    const auto key = uuidKey(uuid);
    return db->Get(
        rocksdb::ReadOptions {}, handle, key, std::forward<Args>(args)...);
}

template<typename... Args>
void BridgeGameRecorder::Impl::merge(
    RecordType recordType, const Uuid& uuid, Args&&... args)
{
    assert(db);
    const auto handle = internalGetColumnFamilyHandle(recordType);
    const auto key = uuidKey(uuid);
    const auto status = db->Merge(
        rocksdb::WriteOptions {}, handle, key, std::forward<Args>(args)...);
    if (!status.ok()) {
        log(LogLevel::WARNING,
            "Failed database merge operation: %s", status.ToString());
    }
}

rocksdb::ColumnFamilyHandle*
BridgeGameRecorder::Impl::internalGetColumnFamilyHandle(
    const RecordType recordType)
{
    const auto n = static_cast<int>(recordType);
    assert(0 <= n && n < ssize(columnFamilyHandles));
    return columnFamilyHandles[n];
}

#else // WITH_RECORDER

struct BridgeGameRecorder::Impl {
    Impl(std::string_view) {}
};

#endif // WITH_RECORDER

BridgeGameRecorder::BridgeGameRecorder(const std::string_view path) :
    impl {std::make_unique<Impl>(std::string {path})}
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
    for (const auto n : to(N_PLAYERS)) {
        if (const auto& u = gameState.playerUuids[n]) {
            game_record.playerUuids[n] = *u;
        }
    }
    if (const auto& u = gameState.dealUuid) {
        game_record.dealUuid = *u;
    }
    impl->put(
        RecordType::GAME, gameUuid, rocksdb::Slice {
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
    for (const auto n : to(N_CARDS)) {
        deal_record.cards[n] = packCard(dereference(deal.getCard(n).getType()));
    }
    deal_record.config = packDealConfig(
        bidding.getOpeningPosition(), deal.getVulnerability());
    impl->put(
        RecordType::DEAL, deal_uuid, rocksdb::Slice {
            reinterpret_cast<const char*>(&deal_record), sizeof(DealRecord)});

    const auto n_calls = bidding.getNumberOfCalls();
    auto calls_record = std::vector<CallRecord>(n_calls, CallRecord {});
    for (const auto n : to(n_calls)) {
        calls_record[n] = packCall(bidding.getCall(n));
    }
    impl->put(
        RecordType::BIDDING, deal_uuid, rocksdb::Slice {
            reinterpret_cast<const char*>(calls_record.data()),
            static_cast<std::size_t>(n_calls)});

    const auto n_tricks = deal.getNumberOfTricks();
    auto tricks_record = std::vector<TrickRecord>(n_tricks, TrickRecord {});
    auto n_cards_in_latest_trick = Trick::N_CARDS_IN_TRICK;
    for (const auto n : to(n_tricks)) {
        const auto& trick = deal.getTrick(n);
        tricks_record[n].leaderPosition = packPosition(
            dereference(deal.getPosition(trick.getLeader())));
        n_cards_in_latest_trick = trick.getNumberOfCardsPlayed();
        for (const auto m : to(n_cards_in_latest_trick)) {
            const auto& card = dereference(trick.getCard(m));
            tricks_record[n].cards[m] = packCard(dereference(card.getType()));
        }
    }
    // We don't record the unplayed cards in the last trick, so for every card
    // not yet played reduce one card record.
    const auto n_recorded_bytes = sizeof(TrickRecord) * n_tricks -
        sizeof(CardRecord) * (Trick::N_CARDS_IN_TRICK - n_cards_in_latest_trick);
    impl->put(
        RecordType::TRICKS, deal_uuid, rocksdb::Slice {
            reinterpret_cast<const char*>(tricks_record.data()),
            n_recorded_bytes});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordPlayer(
    [[maybe_unused]] const Uuid& playerUuid,
    [[maybe_unused]] const Messaging::UserIdView userId)
{
#if WITH_RECORDER
    assert(impl);
    impl->put(RecordType::PLAYER, playerUuid, userId);
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordCall(
    [[maybe_unused]] const Uuid& dealUuid, [[maybe_unused]] const Call& call)
{
#if WITH_RECORDER
    assert(impl);
    const auto packed_call = packCall(call);
    impl->merge(
        RecordType::BIDDING, dealUuid, rocksdb::Slice {
            reinterpret_cast<const char*>(&packed_call), sizeof(packed_call)});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordTrick(
    [[maybe_unused]] const Uuid& dealUuid,
    [[maybe_unused]] const Position leaderPosition)
{
#if WITH_RECORDER
    assert(impl);
    const auto packed_leader_position = packPosition(leaderPosition);
    impl->merge(
        RecordType::TRICKS, dealUuid, rocksdb::Slice {
            reinterpret_cast<const char*>(&packed_leader_position),
            sizeof(packed_leader_position)});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordCard(
    [[maybe_unused]] const Uuid& dealUuid,
    [[maybe_unused]] const CardType& card)
{
#if WITH_RECORDER
    assert(impl);
    const auto packed_card = packCard(card);
    impl->merge(
        RecordType::TRICKS, dealUuid, rocksdb::Slice {
            reinterpret_cast<const char*>(&packed_card), sizeof(packed_card)});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordDealStarted(
    [[maybe_unused]] const Uuid& gameUuid,
    [[maybe_unused]] const Uuid& dealUuid)
{
#if WITH_RECORDER
    assert(impl);
    impl->merge(
        RecordType::DEAL_RESULT, gameUuid, rocksdb::Slice {
            reinterpret_cast<const char*>(&dealUuid), sizeof(dealUuid)});
#endif // WITH_RECORDER
}

void BridgeGameRecorder::recordDealEnded(
    [[maybe_unused]] const Uuid& gameUuid,
    [[maybe_unused]] const DuplicateResult& result)
{
#if WITH_RECORDER
    assert(impl);
    const auto north_south_score =
        static_cast<PackedScore>(
            getPartnershipScore(result, Partnerships::NORTH_SOUTH));
    impl->merge(
        RecordType::DEAL_RESULT, gameUuid, rocksdb::Slice {
            reinterpret_cast<const char*>(&north_south_score),
            sizeof(north_south_score)});
#endif // WITH_RECORDER
}

std::optional<BridgeGameRecorder::GameState>
BridgeGameRecorder::recallGame([[maybe_unused]] const Uuid& gameUuid)
{
#if WITH_RECORDER
    assert(impl);
    auto serialized_record = std::string {};
    const auto status = impl->get(
        RecordType::GAME, gameUuid, &serialized_record);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            log(LogLevel::WARNING, "Error while recalling game %s: %s",
                gameUuid, status.ToString());
        }
        return std::nullopt;
    }
    if (serialized_record.size() < sizeof(GameRecord)) {
        log(LogLevel::WARNING,
            "Unexpected record size while recalling game %s", gameUuid);
        return std::nullopt;
    }
    static_assert( std::is_trivially_copyable_v<GameRecord> );
    auto game_record = GameRecord {};
    std::memmove(&game_record, serialized_record.data(), sizeof(GameRecord));
    auto game_state = BridgeGameRecorder::GameState {};
    for (const auto n : to(N_PLAYERS)) {
        if (!game_record.playerUuids[n].is_nil()) {
            game_state.playerUuids[n] = game_record.playerUuids[n];
        }
    }
    if (!game_record.dealUuid.is_nil()) {
        game_state.dealUuid = game_record.dealUuid;
    }
    return game_state;
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
    auto status = impl->get(
        RecordType::DEAL, dealUuid, &serialized_record);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            log(LogLevel::WARNING, "Error while recalling deal %s: %s",
                dealUuid, status.ToString());
        }
        return std::nullopt;
    }
    if (serialized_record.size() < sizeof(DealRecord)) {
        log(LogLevel::WARNING,
            "Unexpected record size while recalling deal %s", dealUuid);
        return std::nullopt;
    }
    auto deal_record = DealRecord {};
    static_assert( std::is_trivially_copyable_v<DealRecord> );
    std::memmove(&deal_record, serialized_record.data(), sizeof(DealRecord));

    status = impl->get(
        RecordType::BIDDING, dealUuid, &serialized_record);
    if (!status.ok()) {
        log(LogLevel::WARNING, "Error while recalling bidding in deal %s: %s",
            dealUuid, status.ToString());
        return std::nullopt;
    }

    auto calls_record = std::vector<CallRecord>(
        serialized_record.size() / sizeof(CallRecord), CallRecord {});
    static_assert( std::is_trivially_copyable_v<CallRecord> );
    std::memmove(
        calls_record.data(), serialized_record.data(),
        serialized_record.size());

    status = impl->get(
        RecordType::TRICKS, dealUuid, &serialized_record);
    if (!status.ok()) {
        log(LogLevel::WARNING, "Error while recalling tricks in deal %s: %s",
            dealUuid, status.ToString());
        return std::nullopt;
    }
    const auto [tricks_record, last_tricks_record_size] =
        getMultipleRecords<TrickRecord>(serialized_record);
    const auto n_cards_in_last_trick = last_tricks_record_size - 1;

    const auto [openingPosition, vulnerability] = unpackDealConfig(
        deal_record.config);
    return DealState {
        std::make_unique<RecordedDeal>(
            dealUuid, deal_record, std::move(calls_record),
            std::move(tricks_record),
            n_cards_in_last_trick),
        std::make_unique<PeerlessCardProtocol>(
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
    auto user_id = Bridge::Messaging::UserId {};
    const auto status = impl->get(RecordType::PLAYER, playerUuid, &user_id);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            log(LogLevel::WARNING, "Error while recalling player %s: %s",
                playerUuid, status.ToString());
        }
        return std::nullopt;
    }
    return user_id;
#else // WITH_RECORDER
    return std::nullopt;
#endif // WITH_RECORDER
}

std::vector<BridgeGameRecorder::DealResult>
BridgeGameRecorder::recallDealResults([[maybe_unused]] const Uuid& gameUuid)
{
#if WITH_RECORDER
    assert(impl);
    auto serialized_record = std::string {};
    const auto status = impl->get(
        RecordType::DEAL_RESULT, gameUuid, &serialized_record);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            log(LogLevel::WARNING, "Error while recalling deal result %s: %s",
                gameUuid, status.ToString());
        }
        return {};
    }

    const auto [deal_results_record, last_deal_results_record_size] =
        getMultipleRecords<DealResultRecord>(serialized_record);
    const auto last_record_has_result =
        (last_deal_results_record_size == sizeof(DealResultRecord));

    auto deal_results = std::vector<DealResult>(
        deal_results_record.size(), DealResult {});
    for (const auto [n, deal_result_record] : enumerate(deal_results_record)) {
        deal_results[n].dealUuid = deal_result_record.dealUuid;
        if (last_record_has_result || n < ssize(deal_results_record) - 1) {
            deal_results[n].result = makeDuplicateResult(
                Partnerships::NORTH_SOUTH,
                deal_result_record.northSouthScore);
        }
    }

    return deal_results;
#else // WITH_RECORDER
    return {};
#endif // WITH_RECORDER
}

}
}
