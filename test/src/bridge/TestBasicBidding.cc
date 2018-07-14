#include "bridge/BasicBidding.hh"
#include "bridge/Bid.hh"
#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"
#include "Enumerate.hh"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

using Bridge::BasicBidding;
using Bridge::Bid;
using Bridge::Call;
using Bridge::Contract;
using Bridge::Double;
using Bridge::Doubling;
using Bridge::Pass;
using Bridge::Position;
using Bridge::Redouble;
using Bridge::Strain;
using Bridge::enumerate;

namespace {
constexpr Bid BID {2, Strain::CLUBS};
constexpr Bid HIGHER_BID {7, Strain::NO_TRUMP};
}

class BasicBiddingTest : public testing::Test {
protected:
    void makeCallsHelper(const std::vector<Call>& calls)
    {
        for (const auto e : enumerate(calls)) {
            const auto position = clockwise(Position::NORTH, e.first);
            EXPECT_EQ(position, bidding.getPositionInTurn());
            EXPECT_TRUE(bidding.call(position, e.second));
        }
    }

    void expectContractEquals(const Bid& bid, const Doubling doubling)
    {
        EXPECT_EQ(std::make_optional(Contract(bid, doubling)),
                  bidding.getContract());
    }

    void expectDeclarerPositionIs(const Position position)
    {
        EXPECT_EQ(std::make_optional(position),
                  bidding.getDeclarerPosition());
    }

    BasicBidding bidding {Position::NORTH};
};

TEST_F(BasicBiddingTest, testInitialNumberOfCalls)
{
    EXPECT_EQ(0u, bidding.getNumberOfCalls());
}

TEST_F(BasicBiddingTest, testOpeningPosition)
{
    EXPECT_EQ(Position::NORTH, bidding.getOpeningPosition());
}

TEST_F(BasicBiddingTest, testCallsAfterOpeningHasBeenMade)
{
    bidding.call(Position::NORTH, BID);
    EXPECT_EQ(1u, bidding.getNumberOfCalls());
    EXPECT_EQ(Bridge::Call {BID}, bidding.getCall(0));
}

TEST_F(BasicBiddingTest, testPassOut)
{
    const std::vector<Call> calls(4, Pass{});
    makeCallsHelper(calls);
    EXPECT_EQ(std::make_optional(std::optional<Contract>(std::nullopt)),
              bidding.getContract());
}

TEST_F(BasicBiddingTest, testOnlyBidWinsContract)
{
    const auto calls = std::vector<Call>{BID, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(BID, Doubling::UNDOUBLED);
}

TEST_F(BasicBiddingTest, testHighestBidWinsContract)
{
    const auto calls = std::vector<Call>{
        BID, HIGHER_BID, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(HIGHER_BID, Doubling::UNDOUBLED);
}

TEST_F(BasicBiddingTest, testSameBidIsNotAllowed)
{
    bidding.call(Position::NORTH, BID);
    EXPECT_FALSE(bidding.call(Position::EAST, BID));
}

TEST_F(BasicBiddingTest, testLowerBidIsNotAllowed)
{
    bidding.call(Position::NORTH, HIGHER_BID);
    EXPECT_FALSE(bidding.call(Position::EAST, BID));
}

TEST_F(BasicBiddingTest, testInitiallyLowestBidIsAllowed)
{
    EXPECT_EQ(Bridge::Bid::LOWEST_BID, bidding.getLowestAllowedBid());
}

TEST_F(BasicBiddingTest, testAfterBiddingTheNextHigherBidIsAllowed)
{
    bidding.call(Position::NORTH, BID);
    EXPECT_EQ(nextHigherBid(BID), bidding.getLowestAllowedBid());
}

TEST_F(BasicBiddingTest, testEmptyContractMayNotBeDoubled)
{
    EXPECT_FALSE(bidding.call(Position::NORTH, Double {}));
}

TEST_F(BasicBiddingTest, testOpponentsContractMayBeDoubled)
{
    bidding.call(Position::NORTH, BID);
    EXPECT_TRUE(bidding.call(Position::EAST, Double {}));
}

TEST_F(BasicBiddingTest, testOwnContractMayNotBeDoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Pass {});
    EXPECT_FALSE(bidding.call(Position::SOUTH, Double {}));
}

TEST_F(BasicBiddingTest, testDoubledContractMayNotBeDoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Double {});
    bidding.call(Position::SOUTH, Pass {});
    EXPECT_FALSE(bidding.call(Position::WEST, Double {}));
}

TEST_F(BasicBiddingTest, testDoubledContract)
{
    const auto calls = std::vector<Call>{
        BID, Double {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(BID, Doubling::DOUBLED);
}

TEST_F(BasicBiddingTest, testEmptyContractMayNotBeRedoubled)
{
    EXPECT_FALSE(bidding.call(Position::NORTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testOwnContractMayBeRedoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Double {});
    EXPECT_TRUE(bidding.call(Position::SOUTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testOpponentsContractMayNotBeRedoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Double {});
    bidding.call(Position::SOUTH, Pass {});
    EXPECT_FALSE(bidding.call(Position::WEST, Redouble {}));
}

TEST_F(BasicBiddingTest, testUndoubledContractMayNotBeRedoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Pass {});
    EXPECT_FALSE(bidding.call(Position::SOUTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testRedoubledContractMayNotBeDoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Double {});
    bidding.call(Position::SOUTH, Redouble {});
    EXPECT_FALSE(bidding.call(Position::WEST, Double {}));
}

TEST_F(BasicBiddingTest, testRedoubledContractMayNotBeRedoubled)
{
    bidding.call(Position::NORTH, BID);
    bidding.call(Position::EAST, Double {});
    bidding.call(Position::SOUTH, Redouble {});
    bidding.call(Position::WEST, Pass {});
    EXPECT_FALSE(bidding.call(Position::NORTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testRedoubledContract)
{
    const auto calls = std::vector<Call>{
        BID, Double {}, Redouble {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(BID, Doubling::REDOUBLED);
}

TEST_F(BasicBiddingTest, testOnlyBidderIsDeclarer)
{
    const auto calls = std::vector<Call>{
        BID, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Position::NORTH);
}

TEST_F(BasicBiddingTest, testDeclarerIsInWinningPartnership)
{
    const std::vector<Call> calls { BID, HIGHER_BID, Pass {}, Pass {}, Pass {} };
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Position::EAST);
}

TEST_F(BasicBiddingTest, testFirstToCallStrainIsDeclarer)
{
    const auto calls = std::vector<Call>{
        Bid {1, Strain::HEARTS}, Bid {2, Strain::HEARTS},
        Pass {}, Bid {2, Strain::SPADES},
        Pass {}, Bid {4, Strain::SPADES},
        Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Position::WEST);
}

TEST_F(BasicBiddingTest, testFirstToCallStrainIsDeclarerInDoubledContract)
{
    const auto calls = std::vector<Call>{
        Bid {1, Strain::HEARTS}, Bid {2, Strain::HEARTS},
        Pass {}, Bid {2, Strain::SPADES},
        Pass {}, Bid {4, Strain::SPADES},
        Double {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Position::WEST);
}

TEST_F(BasicBiddingTest, testFirstToCallStrainIsDeclarerInRedoubledContract)
{
    const auto calls = std::vector<Call>{
        Bid {1, Strain::HEARTS}, Bid {2, Strain::HEARTS},
        Pass {}, Bid {2, Strain::SPADES},
        Pass {}, Bid {4, Strain::SPADES},
        Double {}, Redouble {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Position::WEST);
}
