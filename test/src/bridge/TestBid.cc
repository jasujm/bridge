#include "bridge/Bid.hh"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

using Bridge::Bid;

namespace Strains = Bridge::Strains;

TEST(BidTest, testConstruction)
{
    constexpr Bid bid {1, Strains::NO_TRUMP};
    EXPECT_EQ(bid.level, 1);
    EXPECT_EQ(bid.strain, Strains::NO_TRUMP);
}

TEST(BidTest, testLevelBelowLowerBound)
{
    EXPECT_THROW(Bid(Bid::MINIMUM_LEVEL - 1, Strains::NO_TRUMP),
                 std::invalid_argument);
}

TEST(BidTest, testLevelAboveUpperBound)
{
    EXPECT_THROW(Bid(Bid::MAXIMUM_LEVEL + 1, Strains::NO_TRUMP),
                 std::invalid_argument);
}

TEST(BidTest, testNextHigherBidWhenStrainIsNotNoTrumpIncreasesStrain)
{
    EXPECT_EQ(Bid(1, Strains::HEARTS), nextHigherBid(Bid(1, Strains::DIAMONDS)));
}

TEST(BidTest, testNextHigherBidWhenStrainIsNoTrumpIncreasesLevel)
{
    EXPECT_EQ(Bid(2, Strains::CLUBS), nextHigherBid(Bid(1, Strains::NO_TRUMP)));
}

TEST(BidTest, testThereIsNoHigherBidThanHighestBid)
{
    EXPECT_FALSE(nextHigherBid(Bid::HIGHEST_BID));
}

TEST(BidTest, testEquality)
{
    EXPECT_EQ(Bid(1, Strains::CLUBS), Bid(1, Strains::CLUBS));
}

TEST(BidTest, testBidAtLowerLevelIsLessThanBidAtHigherLevel)
{
    EXPECT_LT(Bid(3, Strains::SPADES), Bid(4, Strains::HEARTS));
}

TEST(BidTest, testBidAtHigherStrainIsGreaterThanBidWithLowerStrainAtSameLevel)
{
    EXPECT_GT(Bid(4, Strains::SPADES), Bid(4, Strains::HEARTS));
}

TEST(BidTest, testOutput)
{
    std::ostringstream os;
    os << Bid {2, Strains::CLUBS};
    EXPECT_EQ("2 clubs", os.str());
}
