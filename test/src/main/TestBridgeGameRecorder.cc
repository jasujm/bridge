#if WITH_RECORDER

#include "bridge/CardTypeIterator.hh"
#include "bridge/SimpleCard.hh"
#include "engine/CardManager.hh"
#include "engine/GameManager.hh"
#include "main/BridgeGameRecorder.hh"
#include "main/CardProtocol.hh"
#include "messaging/Identity.hh"
#include "Enumerate.hh"
#include "MockBidding.hh"
#include "MockDeal.hh"
#include "MockHand.hh"
#include "MockTrick.hh"
#include "Utility.hh"

#include <boost/range/combine.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <vector>
#include <string_view>

using Bridge::DuplicateResult;
using Bridge::dereference;
using Bridge::to;

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

using namespace std::string_view_literals;

namespace {

boost::uuids::string_generator gen;
const auto GAME_UUID = gen("177a0bec-b2e0-4569-9842-afc56157b268");
const auto DEAL_UUID = gen("45c49107-6f1b-41be-9441-5c46a65bdbed");
const auto PLAYER_UUID = gen("309a41ca-736f-45ce-9f5e-8e09c25d12c9");
constexpr auto OPENING_POSITION = Bridge::Positions::EAST;
constexpr auto VULNERABILITY = Bridge::Vulnerability {true, true};

struct DataDirectory {
    DataDirectory() :
        path {std::filesystem::temp_directory_path() / "testdb"}
    {
    }

    ~DataDirectory()
    {
        std::filesystem::remove_all(path);
    }

    std::filesystem::path path;
};

auto makeCards()
{
    return std::vector<Bridge::SimpleCard>(
        Bridge::cardTypeIterator(0),
        Bridge::cardTypeIterator(Bridge::N_CARDS));
}

}

class BridgeGameRecorderTest : public testing::Test
{
protected:
    virtual void SetUp()
    {
        ON_CALL(deal, handleGetUuid()).WillByDefault(ReturnRef(DEAL_UUID));
        ON_CALL(deal, handleGetVulnerability())
            .WillByDefault(Return(VULNERABILITY));
        ON_CALL(deal, handleGetCard(_)).WillByDefault(
            Invoke(
                [this](const auto n) -> decltype(auto)
                {
                    return cards.at(n);
                }));
        ON_CALL(deal, handleGetBidding()).WillByDefault(ReturnRef(bidding));
        ON_CALL(bidding, handleGetOpeningPosition())
            .WillByDefault(Return(OPENING_POSITION));
        ON_CALL(bidding, handleGetNumberOfCalls())
            .WillByDefault(Return(ssize(calls)));
        ON_CALL(bidding, handleGetCall(_)).WillByDefault(
            Invoke(
                [this](const auto n) -> decltype(auto)
                {
                    return calls.at(n);
                }
                ));
        ON_CALL(tricks[0], handleGetNumberOfCardsPlayed())
            .WillByDefault(Return(Bridge::N_PLAYERS));
        ON_CALL(tricks[0], handleGetCard(_))
            .WillByDefault(
                Invoke(
                    [this](const auto n) -> decltype(auto)
                    {
                        return cards.at(Bridge::N_CARDS_PER_PLAYER * n);
                    }));
        for (auto& trick : tricks) {
            ON_CALL(trick, handleGetHand(_)).WillByDefault(
                Invoke(
                    [this](const auto n) -> decltype(auto)
                    {
                        return hands.at(n);
                    }));
        }
        ON_CALL(deal, handleGetNumberOfTricks)
            .WillByDefault(Return(tricks.size()));
        ON_CALL(deal, handleGetHand(_)).WillByDefault(
            Invoke(
                [this](const auto n) -> decltype(auto)
                    {
                        return hands.at(positionOrder(n));
                    }));
        ON_CALL(deal, handleGetTrick(_))
            .WillByDefault(
                Invoke(
                    [this](const auto n) -> decltype(auto)
                    {
                        return tricks.at(n);
                    }));
    }

    const std::vector<Bridge::SimpleCard> cards {makeCards()};
    const std::vector<Bridge::Call> calls {
        Bridge::Bid {1, Bridge::Strains::CLUBS},
        Bridge::Double {},
        Bridge::Redouble {},
        Bridge::Pass {},
        Bridge::Pass {},
        Bridge::Pass {},
    };
    std::array<NiceMock<Bridge::MockHand>, 4> hands {};
    std::array<NiceMock<Bridge::MockTrick>, 2> tricks {};
    NiceMock<Bridge::MockDeal> deal;
    NiceMock<Bridge::MockBidding> bidding;
    DataDirectory dataDir;
    Bridge::Main::BridgeGameRecorder recorder {dataDir.path.string()};
};

TEST_F(BridgeGameRecorderTest, testBridgeGameRecorderGameNotFound)
{
    EXPECT_EQ(std::nullopt, recorder.recallGame(GAME_UUID));
}

TEST_F(BridgeGameRecorderTest, testBridgeGameRecorderGameFound)
{
    const auto game_state = Bridge::Main::BridgeGameRecorder::GameState {
        { PLAYER_UUID, std::nullopt, PLAYER_UUID, std::nullopt },
        DEAL_UUID,
    };
    recorder.recordGame(GAME_UUID, game_state);
    const auto recalled_game_state = recorder.recallGame(GAME_UUID);
    ASSERT_NE(std::nullopt, recalled_game_state);
    EXPECT_EQ(DEAL_UUID, recalled_game_state->dealUuid);
    EXPECT_TRUE(
        std::equal(
            std::begin(game_state.playerUuids),
            std::end(game_state.playerUuids),
            std::begin(recalled_game_state->playerUuids),
            std::end(recalled_game_state->playerUuids)));
}

TEST_F(BridgeGameRecorderTest, testBridgeGameRecorderDealNotFound)
{
    EXPECT_EQ(std::nullopt, recorder.recallDeal(DEAL_UUID));
}

TEST_F(BridgeGameRecorderTest, testBridgeGameRecorderDealFound)
{
    recorder.recordDeal(deal);
    const auto record = recorder.recallDeal(DEAL_UUID);
    const auto& [recalledDeal, recalledCardProtocol, recalledGameManager] =
        dereference(record);
    ASSERT_NE(nullptr, recalledDeal);
    EXPECT_EQ(DEAL_UUID, recalledDeal->getUuid());
    EXPECT_EQ(VULNERABILITY, recalledDeal->getVulnerability());
    for (const auto n : Bridge::to(Bridge::N_CARDS)) {
        EXPECT_EQ(cards[n].getType(), recalledDeal->getCard(n).getType());
    }
    const auto& recalledBidding = recalledDeal->getBidding();
    EXPECT_EQ(
        std::vector(bidding.begin(), bidding.end()),
        std::vector(recalledBidding.begin(), recalledBidding.end()));

    const auto& trick1 = recalledDeal->getTrick(0);
    const auto iter = trick1.begin();
    ASSERT_EQ(4, trick1.end() - iter);
    EXPECT_EQ(&iter->first,      &recalledDeal->getHand(Bridge::Positions::NORTH));
    EXPECT_EQ(&iter->second,     &recalledDeal->getCard(0));
    EXPECT_EQ(&(iter+1)->first,  &recalledDeal->getHand(Bridge::Positions::EAST));
    EXPECT_EQ(&(iter+1)->second, &recalledDeal->getCard(Bridge::N_CARDS_PER_PLAYER));
    EXPECT_EQ(&(iter+2)->first,  &recalledDeal->getHand(Bridge::Positions::SOUTH));
    EXPECT_EQ(&(iter+2)->second, &recalledDeal->getCard(2 * Bridge::N_CARDS_PER_PLAYER));
    EXPECT_EQ(&(iter+3)->first,  &recalledDeal->getHand(Bridge::Positions::WEST));
    EXPECT_EQ(&(iter+3)->second, &recalledDeal->getCard(3 * Bridge::N_CARDS_PER_PLAYER));

    const auto& trick2 = recalledDeal->getTrick(1);
    EXPECT_EQ(trick2.end(), trick2.begin());

    ASSERT_NE(nullptr, recalledCardProtocol);
    const auto recalledCardManager = recalledCardProtocol->getCardManager();
    ASSERT_NE(nullptr, recalledCardManager);
    for (const auto n : to(Bridge::N_CARDS)) {
        EXPECT_EQ(
            Bridge::enumerateCardType(n),
            dereference(recalledCardManager->getCard(n)).getType());
    }

    ASSERT_NE(nullptr, recalledGameManager);
    EXPECT_EQ(OPENING_POSITION, recalledGameManager->getOpenerPosition());
    EXPECT_EQ(VULNERABILITY, recalledGameManager->getVulnerability());
}

TEST_F(BridgeGameRecorderTest, testRecordCall)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(ssize(calls) - 1));
    EXPECT_CALL(deal, handleGetNumberOfTricks()).WillRepeatedly(Return(0));
    recorder.recordDeal(deal);
    recorder.recordCall(DEAL_UUID, calls.back());
    const auto record = recorder.recallDeal(DEAL_UUID);
    const auto& recalledDeal = dereference(record).deal;
    ASSERT_NE(nullptr, recalledDeal);
    const auto& recalledBidding = recalledDeal->getBidding();
    testing::Mock::VerifyAndClearExpectations(&bidding);

    EXPECT_EQ(
        std::vector(bidding.begin(), bidding.end()),
        std::vector(recalledBidding.begin(), recalledBidding.end()));
    EXPECT_EQ(0, recalledDeal->getNumberOfTricks());
}

TEST_F(BridgeGameRecorderTest, testRecordTrick)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks()).WillRepeatedly(Return(0));
    recorder.recordDeal(deal);
    recorder.recordTrick(DEAL_UUID, Bridge::Positions::NORTH);
    for (const auto n : to(Bridge::N_PLAYERS)) {
        const auto n_player = (n + 0) % Bridge::N_PLAYERS;
        recorder.recordCard(
            DEAL_UUID,
            dereference(
                cards.at(n_player * Bridge::N_CARDS_PER_PLAYER).getType()));
    }

    const auto record = recorder.recallDeal(DEAL_UUID);
    const auto& recalledDeal = dereference(record).deal;
    ASSERT_NE(nullptr, recalledDeal);

    ASSERT_EQ(1, recalledDeal->getNumberOfTricks());
    const auto& recalledTrick = recalledDeal->getTrick(0);
    const auto iter = recalledTrick.begin();
    ASSERT_EQ(4, recalledTrick.end() - iter);
    EXPECT_EQ(&iter->first,      &recalledDeal->getHand(Bridge::Positions::NORTH));
    EXPECT_EQ(&iter->second,     &recalledDeal->getCard(0));
    EXPECT_EQ(&(iter+1)->first,  &recalledDeal->getHand(Bridge::Positions::EAST));
    EXPECT_EQ(&(iter+1)->second, &recalledDeal->getCard(Bridge::N_CARDS_PER_PLAYER));
    EXPECT_EQ(&(iter+2)->first,  &recalledDeal->getHand(Bridge::Positions::SOUTH));
    EXPECT_EQ(&(iter+2)->second, &recalledDeal->getCard(2 * Bridge::N_CARDS_PER_PLAYER));
    EXPECT_EQ(&(iter+3)->first,  &recalledDeal->getHand(Bridge::Positions::WEST));
    EXPECT_EQ(&(iter+3)->second, &recalledDeal->getCard(3 * Bridge::N_CARDS_PER_PLAYER));

    // The first cards in each hand were played in the trick
    for (const auto position : Bridge::Position::all()) {
        const auto& hand = recalledDeal->getHand(position);
        for (const auto n : Bridge::to(Bridge::N_CARDS_PER_PLAYER)) {
            EXPECT_EQ(n == 0, hand.isPlayed(n));
        }
    }
}

TEST_F(BridgeGameRecorderTest, testPlayerNotFound)
{
    EXPECT_EQ(std::nullopt, recorder.recallPlayer(PLAYER_UUID));
}

TEST_F(BridgeGameRecorderTest, testPlayerFound)
{
    const auto userId = Bridge::Messaging::UserId {"user"};
    recorder.recordPlayer(PLAYER_UUID, userId);
    EXPECT_EQ(userId, recorder.recallPlayer(PLAYER_UUID));
}

TEST_F(BridgeGameRecorderTest, testDatabaseFailure)
{
    EXPECT_THROW(
        Bridge::Main::BridgeGameRecorder("/dev/null"sv),
        std::runtime_error);
}

TEST_F(BridgeGameRecorderTest, testEmptyDealResults)
{
    const auto dealResults = recorder.recallDealResults(GAME_UUID);
    EXPECT_TRUE(dealResults.empty());
}

TEST_F(BridgeGameRecorderTest, testDealResultsIncompleteDeal)
{
    recorder.recordDealStarted(GAME_UUID, DEAL_UUID);
    const auto deal_results = recorder.recallDealResults(GAME_UUID);
    ASSERT_EQ(1u, deal_results.size());
    const auto& deal_result = deal_results.front();
    EXPECT_EQ(DEAL_UUID, deal_result.dealUuid);
    EXPECT_EQ(std::nullopt, deal_result.result);
}

TEST_F(BridgeGameRecorderTest, testDealResultsCompleteDeal)
{
    constexpr auto DUPLICATE_RESULTS = std::array {
        DuplicateResult {Bridge::Partnerships::NORTH_SOUTH, 100},
        DuplicateResult {Bridge::Partnerships::EAST_WEST, 200},
        Bridge::DuplicateResult::passedOut(),
    };
    for (const auto& result : DUPLICATE_RESULTS) {
        recorder.recordDealStarted(GAME_UUID, DEAL_UUID);
        recorder.recordDealEnded(GAME_UUID, result);
    }
    const auto deal_results = recorder.recallDealResults(GAME_UUID);
    ASSERT_EQ(DUPLICATE_RESULTS.size(), deal_results.size());
    for (const auto t : boost::combine(deal_results, DUPLICATE_RESULTS)) {
        const auto& deal_result = t.get<0>();
        const auto& duplicate_result = t.get<1>();
        EXPECT_EQ(DEAL_UUID, deal_result.dealUuid);
        EXPECT_EQ(duplicate_result, deal_result.result);
    }
}

#endif // WITH_RECORDER
