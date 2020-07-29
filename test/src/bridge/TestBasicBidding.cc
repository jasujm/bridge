#include "bridge/BasicBidding.hh"
#include "bridge/Bid.hh"
#include "bridge/Call.hh"
#include "bridge/Position.hh"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

class BasicBiddingTest : public testing::Test {
protected:
    Bridge::BasicBidding bidding {Bridge::Positions::NORTH};
};

TEST_F(BasicBiddingTest, testInitialNumberOfCalls)
{
    EXPECT_EQ(0, bidding.getNumberOfCalls());
}

TEST_F(BasicBiddingTest, testOpeningPosition)
{
    EXPECT_EQ(Bridge::Positions::NORTH, bidding.getOpeningPosition());
}

TEST_F(BasicBiddingTest, testCallsAfterOpeningHasBeenMade)
{
    const auto BID = Bridge::Bid {4, Bridge::Strains::HEARTS};
    bidding.call(Bridge::Positions::NORTH, BID);
    ASSERT_EQ(1, bidding.getNumberOfCalls());
    EXPECT_EQ(Bridge::Call {BID}, bidding.getCall(0));
}
