#include "bridge/AllowedCalls.hh"
#include "bridge/Bid.hh"
#include "bridge/Call.hh"
#include "MockBidding.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <vector>

using Bridge::BidIterator;
using Bridge::Bid;
using Bridge::Call;

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

constexpr Bid LOWEST_ALLOWED_BID {2, Bridge::Strains::DIAMONDS};
const Call PASS_CALL {Bridge::Pass {}};
const Call DOUBLE_CALL {Bridge::Double {}};
const Call REDOUBLE_CALL {Bridge::Redouble {}};

bool compareCalls(const Call& call1, const Call& call2)
{
    return call1 < call2;
}

}

class AllowedCallsTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(bidding, handleHasEnded()).WillByDefault(Return(false));
        ON_CALL(bidding, handleGetLowestAllowedBid())
            .WillByDefault(Return(LOWEST_ALLOWED_BID));
        ON_CALL(bidding, handleIsCallAllowed(_))
            .WillByDefault(Return(false));
    }

    testing::NiceMock<Bridge::MockBidding> bidding;
    std::vector<Call> calls;
};

TEST_F(AllowedCallsTest, testPassIsAlwaysAllowed)
{
    getAllowedCalls(bidding, std::back_inserter(calls));
    EXPECT_NE(calls.end(), std::find(calls.begin(), calls.end(), PASS_CALL));
}

TEST_F(AllowedCallsTest, testDoublingNotAllowed)
{
    getAllowedCalls(bidding, std::back_inserter(calls));
    EXPECT_EQ(calls.end(), std::find(calls.begin(), calls.end(), DOUBLE_CALL));
}

TEST_F(AllowedCallsTest, testDoublingAllowed)
{
    ON_CALL(bidding, handleIsCallAllowed(DOUBLE_CALL))
        .WillByDefault(Return(true));
    getAllowedCalls(bidding, std::back_inserter(calls));
    EXPECT_NE(calls.end(), std::find(calls.begin(), calls.end(), DOUBLE_CALL));
}

TEST_F(AllowedCallsTest, testRedoublingNotAllowed)
{
    getAllowedCalls(bidding, std::back_inserter(calls));
    EXPECT_EQ(
        calls.end(), std::find(calls.begin(), calls.end(), REDOUBLE_CALL));
}

TEST_F(AllowedCallsTest, testRedoublingAllowed)
{
    ON_CALL(bidding, handleIsCallAllowed(REDOUBLE_CALL))
        .WillByDefault(Return(true));
    getAllowedCalls(bidding, std::back_inserter(calls));
    EXPECT_NE(
        calls.end(), std::find(calls.begin(), calls.end(), REDOUBLE_CALL));
}

TEST_F(AllowedCallsTest, testNonAllowedBids)
{
    getAllowedCalls(bidding, std::back_inserter(calls));
    std::sort(calls.begin(), calls.end());
    auto intersection = std::vector<Call> {};
    std::set_intersection(
        calls.begin(), calls.end(),
        BidIterator(Bid::LOWEST_BID), BidIterator(LOWEST_ALLOWED_BID),
        std::back_inserter(intersection), compareCalls);
    EXPECT_TRUE(intersection.empty());
}

TEST_F(AllowedCallsTest, testAllowedBids)
{
    getAllowedCalls(bidding, std::back_inserter(calls));
    std::sort(calls.begin(), calls.end());
    EXPECT_TRUE(
        std::includes(
            calls.begin(), calls.end(),
            BidIterator(LOWEST_ALLOWED_BID), BidIterator(std::nullopt),
            compareCalls));
}

TEST_F(AllowedCallsTest, testPositionNotInTurn)
{
    ON_CALL(bidding, handleHasEnded()).WillByDefault(Return(true));
    getAllowedCalls(bidding, std::back_inserter(calls));
    EXPECT_TRUE(calls.empty());
}
