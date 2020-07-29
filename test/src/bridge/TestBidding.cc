#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"
#include "Utility.hh"
#include "MockBidding.hh"

#include <boost/logic/tribool.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <vector>
#include <utility>

using Bridge::Bid;
using Bridge::Bidding;
using Bridge::Call;
using Bridge::Contract;
using Bridge::N_PLAYERS;

using testing::_;
using testing::Ge;
using testing::Return;

namespace Doublings = Bridge::Doublings;
namespace Positions = Bridge::Positions;
namespace Strains = Bridge::Strains;

namespace {
constexpr auto BID = Bid {2, Strains::CLUBS};
constexpr auto OVERBID = Bid {3, Strains::CLUBS};
constexpr auto HIGHER_BID = Bid {7, Strains::NO_TRUMP};
const auto PASS = Bridge::Pass {};
const auto DOUBLE = Bridge::Double {};
const auto REDOUBLE = Bridge::Redouble {};
}

class BiddingTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        ON_CALL(bidding, handleGetOpeningPosition())
            .WillByDefault(Return(Positions::NORTH));
    }

    testing::NiceMock<Bridge::MockBidding> bidding;
};

TEST_F(BiddingTest, testNumberOfCalls)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(1));
    EXPECT_EQ(1, bidding.getNumberOfCalls());
}

TEST_F(BiddingTest, testOpeningPosition)
{
    EXPECT_EQ(Positions::NORTH, bidding.getOpeningPosition());
}

TEST_F(BiddingTest, testGetCallInRange)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_EQ(Call(BID), bidding.getCall(0));
}

TEST_F(BiddingTest, testGetCallOutOfRange)
{
    EXPECT_THROW(bidding.getCall(0), std::out_of_range);
}

TEST_F(BiddingTest, testInitialPositionInTurn)
{
    EXPECT_EQ(Positions::NORTH, bidding.getPositionInTurn());
}

TEST_F(BiddingTest, testInitialPositionInTurnAfterOpening)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_EQ(Positions::EAST, bidding.getPositionInTurn());
}

TEST_F(BiddingTest, testInitialPositionInAfterEnd)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS));
    EXPECT_CALL(bidding, handleGetCall(_)).WillRepeatedly(Return(PASS));
    EXPECT_EQ(std::nullopt, bidding.getPositionInTurn());
}

TEST_F(BiddingTest, testInitialState)
{
    EXPECT_FALSE(bidding.hasEnded());
    EXPECT_TRUE(boost::logic::indeterminate(bidding.hasContract()));
    EXPECT_EQ(std::nullopt, bidding.getContract());
    EXPECT_EQ(std::nullopt, bidding.getDeclarerPosition());
}

TEST_F(BiddingTest, testThreePassesRequiredForCompletion)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(PASS));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(Ge(2))).WillRepeatedly(Return(PASS));
    EXPECT_FALSE(bidding.hasEnded());
}

TEST_F(BiddingTest, testPassOut)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS));
    EXPECT_CALL(bidding, handleGetCall(_)).WillRepeatedly(Return(PASS));
    EXPECT_TRUE(bidding.hasEnded());
    EXPECT_EQ(false, bidding.hasContract());
    EXPECT_EQ(std::nullopt, dereference(bidding.getContract()));
    EXPECT_EQ(std::nullopt, dereference(bidding.getDeclarerPosition()));
}

TEST_F(BiddingTest, testSoleBidWinsContract)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(Ge(1))).WillRepeatedly(Return(PASS));
    EXPECT_TRUE(bidding.hasEnded());
    EXPECT_EQ(true, bidding.hasContract());
    EXPECT_EQ(
        Contract(BID, Doublings::UNDOUBLED),
        dereference(bidding.getContract()));
    EXPECT_EQ(Positions::NORTH, dereference(bidding.getDeclarerPosition()));
}

TEST_F(BiddingTest, testHighestBidWinsContract)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS + 1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(HIGHER_BID));
    EXPECT_CALL(bidding, handleGetCall(Ge(2))).WillRepeatedly(Return(PASS));
    EXPECT_TRUE(bidding.hasEnded());
    EXPECT_EQ(true, bidding.hasContract());
    EXPECT_EQ(
        Contract(HIGHER_BID, Doublings::UNDOUBLED),
        dereference(bidding.getContract()));
    EXPECT_EQ(Positions::EAST, dereference(bidding.getDeclarerPosition()));
}

TEST_F(BiddingTest, testDoubledContract)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS + 1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(DOUBLE));
    EXPECT_CALL(bidding, handleGetCall(Ge(2))).WillRepeatedly(Return(PASS));
    EXPECT_EQ(
        Contract(BID, Doublings::DOUBLED), dereference(bidding.getContract()));
}

TEST_F(BiddingTest, testRedoubledContract)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS + 2));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(DOUBLE));
    EXPECT_CALL(bidding, handleGetCall(2)).WillRepeatedly(Return(REDOUBLE));
    EXPECT_CALL(bidding, handleGetCall(Ge(3))).WillRepeatedly(Return(PASS));
    EXPECT_EQ(
        Contract(BID, Doublings::REDOUBLED), dereference(bidding.getContract()));
}

TEST_F(BiddingTest, testNewBidResetsDouble)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS + 2));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(DOUBLE));
    EXPECT_CALL(bidding, handleGetCall(2)).WillRepeatedly(Return(HIGHER_BID));
    EXPECT_CALL(bidding, handleGetCall(Ge(3))).WillRepeatedly(Return(PASS));
    EXPECT_TRUE(bidding.hasEnded());
    EXPECT_EQ(true, bidding.hasContract());
    EXPECT_EQ(
        Contract(HIGHER_BID, Doublings::UNDOUBLED),
        dereference(bidding.getContract()));
    EXPECT_EQ(Positions::SOUTH, dereference(bidding.getDeclarerPosition()));
}

TEST_F(BiddingTest, testFirstToBidStrainIsDeclarer)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS + 3));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(PASS));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(2)).WillRepeatedly(Return(PASS));
    EXPECT_CALL(bidding, handleGetCall(3)).WillRepeatedly(Return(OVERBID));
    EXPECT_CALL(bidding, handleGetCall(Ge(4))).WillRepeatedly(Return(PASS));
    EXPECT_EQ(Positions::EAST, dereference(bidding.getDeclarerPosition()));
}

TEST_F(BiddingTest, testPassAllowedDuringBidding)
{
    EXPECT_CALL(bidding, handleAddCall(Call {PASS}));
    EXPECT_TRUE(bidding.call(Positions::NORTH, PASS));
}

TEST_F(BiddingTest, testInitialLowestAllowedBid)
{
    EXPECT_EQ(Bid::LOWEST_BID, bidding.getLowestAllowedBid());
}

TEST_F(BiddingTest, testInitialDoublingNotAllowed)
{
    EXPECT_FALSE(bidding.isDoublingAllowed());
}

TEST_F(BiddingTest, testInitialRedoublingNotAllowed)
{
    EXPECT_FALSE(bidding.isRedoublingAllowed());
}

TEST_F(BiddingTest, testOpeningBid)
{
    EXPECT_CALL(bidding, handleAddCall(Call {BID}));
    EXPECT_TRUE(bidding.call(Positions::NORTH, BID));
}

TEST_F(BiddingTest, testSameBidNotAllowed)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_EQ(nextHigherBid(BID), bidding.getLowestAllowedBid());
    EXPECT_FALSE(bidding.call(Positions::EAST, BID));
}

TEST_F(BiddingTest, testHigherBidAllowed)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleAddCall(Call {HIGHER_BID}));
    EXPECT_TRUE(bidding.call(Positions::EAST, HIGHER_BID));
}

TEST_F(BiddingTest, testDoubling)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(1));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleAddCall(Call {DOUBLE}));
    EXPECT_TRUE(bidding.isDoublingAllowed());
    EXPECT_TRUE(bidding.call(Positions::EAST, DOUBLE));
}

TEST_F(BiddingTest, testDoublingPartnerNotAllowed)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(2));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(PASS));
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_FALSE(bidding.isDoublingAllowed());
    EXPECT_FALSE(bidding.call(Positions::SOUTH, DOUBLE));
}

TEST_F(BiddingTest, testRedoubling)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(2));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(DOUBLE));
    EXPECT_CALL(bidding, handleAddCall(Call {REDOUBLE}));
    EXPECT_TRUE(bidding.isRedoublingAllowed());
    EXPECT_TRUE(bidding.call(Positions::SOUTH, REDOUBLE));
}

TEST_F(BiddingTest, testRedoublingOpponentNotAllowed)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(3));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(1)).WillRepeatedly(Return(DOUBLE));
    EXPECT_CALL(bidding, handleGetCall(2)).WillRepeatedly(Return(PASS));
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_FALSE(bidding.isRedoublingAllowed());
    EXPECT_FALSE(bidding.call(Positions::WEST, REDOUBLE));
}

TEST_F(BiddingTest, testCallOutOfTurn)
{
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_FALSE(bidding.call(Positions::EAST, PASS));
}

TEST_F(BiddingTest, testCallNotAllowedAfterBidding)
{
    EXPECT_CALL(
        bidding, handleGetNumberOfCalls()).WillRepeatedly(Return(N_PLAYERS));
    EXPECT_CALL(bidding, handleGetCall(_)).WillRepeatedly(Return(PASS));
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_EQ(std::nullopt, bidding.getLowestAllowedBid());
    EXPECT_FALSE(bidding.isDoublingAllowed());
    EXPECT_FALSE(bidding.isRedoublingAllowed());
    EXPECT_FALSE(bidding.call(Positions::NORTH, PASS));
}

TEST_F(BiddingTest, testCallIterators)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls())
        .WillRepeatedly(Return(N_PLAYERS));
    EXPECT_CALL(bidding, handleGetCall(0)).WillRepeatedly(Return(BID));
    EXPECT_CALL(bidding, handleGetCall(Ge(1))).WillRepeatedly(Return(PASS));
    EXPECT_THAT(
        std::vector(bidding.begin(), bidding.end()),
        testing::ElementsAre(
            std::pair {Positions::NORTH, BID},
            std::pair {Positions::EAST,  PASS},
            std::pair {Positions::SOUTH, PASS},
            std::pair {Positions::WEST,  PASS}));
}
