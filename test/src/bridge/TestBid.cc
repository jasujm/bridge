#include "bridge/Bid.hh"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

using Bridge::Bid;
using Bridge::Strain;

TEST(BidTest, testConstruction)
{
    constexpr Bid bid {1, Strain::NO_TRUMP};
    EXPECT_EQ(bid.level, 1);
    EXPECT_EQ(bid.strain, Strain::NO_TRUMP);
}

TEST(BidTest, testLevelBelowLowerBound)
{
    EXPECT_THROW(Bid(Bid::MINIMUM_LEVEL - 1, Strain::NO_TRUMP),
                 std::invalid_argument);
}

TEST(BidTest, testLevelAboveUpperBound)
{
    EXPECT_THROW(Bid(Bid::MAXIMUM_LEVEL + 1, Strain::NO_TRUMP),
                 std::invalid_argument);
}

TEST(BidTest, testNextHigherBidWhenStrainIsNotNoTrumpIncreasesStrain)
{
    EXPECT_EQ(Bid(1, Strain::HEARTS), nextHigherBid(Bid(1, Strain::DIAMONDS)));
}

TEST(BidTest, testNextHigherBidWhenStrainIsNoTrumpIncreasesLevel)
{
    EXPECT_EQ(Bid(2, Strain::CLUBS), nextHigherBid(Bid(1, Strain::NO_TRUMP)));
}

TEST(BidTest, testThereIsNoHigherBidThanHighestBid)
{
    EXPECT_FALSE(nextHigherBid(Bid::HIGHEST_BID));
}

TEST(BidTest, testEquality)
{
    EXPECT_EQ(Bid(1, Strain::CLUBS), Bid(1, Strain::CLUBS));
}

TEST(BidTest, testBidAtLowerLevelIsLessThanBidAtHigherLevel)
{
    EXPECT_LT(Bid(3, Strain::SPADES), Bid(4, Strain::HEARTS));
}

TEST(BidTest, testBidAtHigherStrainIsGreaterThanBidWithLowerStrainAtSameLevel)
{
    EXPECT_GT(Bid(4, Strain::SPADES), Bid(4, Strain::HEARTS));
}

TEST(BidTest, testOutput)
{
    std::ostringstream os;
    os << Bid {2, Strain::CLUBS};
    EXPECT_EQ("2 clubs", os.str());
}
