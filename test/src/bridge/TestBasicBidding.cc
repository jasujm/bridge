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
using Bridge::enumerate;

namespace Doublings = Bridge::Doublings;
namespace Positions = Bridge::Positions;
namespace Strains = Bridge::Strains;

namespace {
constexpr auto BID = Bid {2, Strains::CLUBS};
constexpr auto HIGHER_BID = Bid {7, Strains::NO_TRUMP};
}

class BasicBiddingTest : public testing::Test {
protected:
    void makeCallsHelper(const std::vector<Call>& calls)
    {
        for (const auto e : enumerate(calls)) {
            const auto position = clockwise(Positions::NORTH, e.first);
            EXPECT_EQ(position, bidding.getPositionInTurn());
            EXPECT_TRUE(bidding.call(position, e.second));
        }
    }

    void expectContractEquals(const Bid& bid, const Doubling doubling)
    {
        EXPECT_EQ(
            std::optional(Contract(bid, doubling)),
            bidding.getContract());
    }

    void expectDeclarerPositionIs(const Position position)
    {
        EXPECT_EQ(
            std::optional {position},
            bidding.getDeclarerPosition());
    }

    BasicBidding bidding {Positions::NORTH};
};

TEST_F(BasicBiddingTest, testInitialNumberOfCalls)
{
    EXPECT_EQ(0, bidding.getNumberOfCalls());
}

TEST_F(BasicBiddingTest, testOpeningPosition)
{
    EXPECT_EQ(Positions::NORTH, bidding.getOpeningPosition());
}

TEST_F(BasicBiddingTest, testCallsAfterOpeningHasBeenMade)
{
    bidding.call(Positions::NORTH, BID);
    EXPECT_EQ(1, bidding.getNumberOfCalls());
    EXPECT_EQ(Bridge::Call {BID}, bidding.getCall(0));
}

TEST_F(BasicBiddingTest, testPassOut)
{
    const std::vector<Call> calls(4, Pass{});
    makeCallsHelper(calls);
    EXPECT_EQ(
        std::make_optional(std::optional<Contract> {}),
        bidding.getContract());
}

TEST_F(BasicBiddingTest, testOnlyBidWinsContract)
{
    const auto calls = std::vector<Call>{BID, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(BID, Doublings::UNDOUBLED);
}

TEST_F(BasicBiddingTest, testHighestBidWinsContract)
{
    const auto calls = std::vector<Call>{
        BID, HIGHER_BID, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(HIGHER_BID, Doublings::UNDOUBLED);
}

TEST_F(BasicBiddingTest, testSameBidIsNotAllowed)
{
    bidding.call(Positions::NORTH, BID);
    EXPECT_FALSE(bidding.call(Positions::EAST, BID));
}

TEST_F(BasicBiddingTest, testLowerBidIsNotAllowed)
{
    bidding.call(Positions::NORTH, HIGHER_BID);
    EXPECT_FALSE(bidding.call(Positions::EAST, BID));
}

TEST_F(BasicBiddingTest, testInitiallyLowestBidIsAllowed)
{
    EXPECT_EQ(Bridge::Bid::LOWEST_BID, bidding.getLowestAllowedBid());
}

TEST_F(BasicBiddingTest, testAfterBiddingTheNextHigherBidIsAllowed)
{
    bidding.call(Positions::NORTH, BID);
    EXPECT_EQ(nextHigherBid(BID), bidding.getLowestAllowedBid());
}

TEST_F(BasicBiddingTest, testEmptyContractMayNotBeDoubled)
{
    EXPECT_FALSE(bidding.call(Positions::NORTH, Double {}));
}

TEST_F(BasicBiddingTest, testOpponentsContractMayBeDoubled)
{
    bidding.call(Positions::NORTH, BID);
    EXPECT_TRUE(bidding.call(Positions::EAST, Double {}));
}

TEST_F(BasicBiddingTest, testOwnContractMayNotBeDoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Pass {});
    EXPECT_FALSE(bidding.call(Positions::SOUTH, Double {}));
}

TEST_F(BasicBiddingTest, testDoubledContractMayNotBeDoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Double {});
    bidding.call(Positions::SOUTH, Pass {});
    EXPECT_FALSE(bidding.call(Positions::WEST, Double {}));
}

TEST_F(BasicBiddingTest, testDoubledContract)
{
    const auto calls = std::vector<Call>{
        BID, Double {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(BID, Doublings::DOUBLED);
}

TEST_F(BasicBiddingTest, testEmptyContractMayNotBeRedoubled)
{
    EXPECT_FALSE(bidding.call(Positions::NORTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testOwnContractMayBeRedoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Double {});
    EXPECT_TRUE(bidding.call(Positions::SOUTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testOpponentsContractMayNotBeRedoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Double {});
    bidding.call(Positions::SOUTH, Pass {});
    EXPECT_FALSE(bidding.call(Positions::WEST, Redouble {}));
}

TEST_F(BasicBiddingTest, testUndoubledContractMayNotBeRedoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Pass {});
    EXPECT_FALSE(bidding.call(Positions::SOUTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testRedoubledContractMayNotBeDoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Double {});
    bidding.call(Positions::SOUTH, Redouble {});
    EXPECT_FALSE(bidding.call(Positions::WEST, Double {}));
}

TEST_F(BasicBiddingTest, testRedoubledContractMayNotBeRedoubled)
{
    bidding.call(Positions::NORTH, BID);
    bidding.call(Positions::EAST, Double {});
    bidding.call(Positions::SOUTH, Redouble {});
    bidding.call(Positions::WEST, Pass {});
    EXPECT_FALSE(bidding.call(Positions::NORTH, Redouble {}));
}

TEST_F(BasicBiddingTest, testRedoubledContract)
{
    const auto calls = std::vector<Call>{
        BID, Double {}, Redouble {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectContractEquals(BID, Doublings::REDOUBLED);
}

TEST_F(BasicBiddingTest, testOnlyBidderIsDeclarer)
{
    const auto calls = std::vector<Call>{
        BID, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Positions::NORTH);
}

TEST_F(BasicBiddingTest, testDeclarerIsInWinningPartnership)
{
    const std::vector<Call> calls { BID, HIGHER_BID, Pass {}, Pass {}, Pass {} };
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Positions::EAST);
}

TEST_F(BasicBiddingTest, testFirstToCallStrainIsDeclarer)
{
    const auto calls = std::vector<Call>{
        Bid {1, Strains::HEARTS}, Bid {2, Strains::HEARTS},
        Pass {}, Bid {2, Strains::SPADES},
        Pass {}, Bid {4, Strains::SPADES},
        Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Positions::WEST);
}

TEST_F(BasicBiddingTest, testFirstToCallStrainIsDeclarerInDoubledContract)
{
    const auto calls = std::vector<Call>{
        Bid {1, Strains::HEARTS}, Bid {2, Strains::HEARTS},
        Pass {}, Bid {2, Strains::SPADES},
        Pass {}, Bid {4, Strains::SPADES},
        Double {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Positions::WEST);
}

TEST_F(BasicBiddingTest, testFirstToCallStrainIsDeclarerInRedoubledContract)
{
    const auto calls = std::vector<Call>{
        Bid {1, Strains::HEARTS}, Bid {2, Strains::HEARTS},
        Pass {}, Bid {2, Strains::SPADES},
        Pass {}, Bid {4, Strains::SPADES},
        Double {}, Redouble {}, Pass {}, Pass {}, Pass {}};
    makeCallsHelper(calls);
    expectDeclarerPositionIs(Positions::WEST);
}
