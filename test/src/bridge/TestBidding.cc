#include "bridge/Bid.hh"
#include "bridge/BridgeConstants.hh"
#include "bridge/Call.hh"
#include "bridge/Contract.hh"
#include "bridge/Position.hh"
#include "Utility.hh"
#include "MockBidding.hh"

#include <boost/logic/tribool.hpp>
#include <boost/optional/optional.hpp>
#include <gtest/gtest.h>

#include <stdexcept>

using Bridge::Bidding;
using Bridge::Contract;
using Bridge::Position;
using Bridge::to;

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::Ref;
using testing::ValuesIn;

namespace {
constexpr Contract CONTRACT {
    Bridge::Bid {2, Bridge::Strain::CLUBS},
    Bridge::Doubling::UNDOUBLED,
};
const Bridge::Call VALID_CALL {Bridge::Pass {}};
const Bridge::Call INVALID_CALL {Bridge::Double {}};
}

class BiddingTest : public testing::TestWithParam<std::size_t> {
protected:
    virtual void SetUp()
    {
        ON_CALL(bidding, handleIsCallAllowed(VALID_CALL))
            .WillByDefault(Return(true));
        ON_CALL(bidding, handleGetOpeningPosition())
            .WillByDefault(Return(Position::NORTH));
        ON_CALL(bidding, handleGetCall(_))
            .WillByDefault(Return(VALID_CALL));
        ON_CALL(bidding, handleIsCallAllowed(INVALID_CALL))
            .WillByDefault(Return(false));
        ON_CALL(bidding, handleHasContract())
            .WillByDefault(Return(true));
        ON_CALL(bidding, handleGetContract())
            .WillByDefault(Return(CONTRACT));
        ON_CALL(bidding, handleGetDeclarerPosition())
            .WillByDefault(Return(Position::NORTH));
    }

    NiceMock<Bridge::MockBidding> bidding;
};

TEST_F(BiddingTest, testNumberOfCalls)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillOnce(Return(1));
    EXPECT_EQ(1u, bidding.getNumberOfCalls());
}

TEST_F(BiddingTest, testOpeningPosition)
{
    EXPECT_CALL(bidding, handleGetOpeningPosition());
    EXPECT_EQ(Position::NORTH, bidding.getOpeningPosition());
}

TEST_F(BiddingTest, testGetCallInRange)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillOnce(Return(1));
    EXPECT_CALL(bidding, handleGetCall(0));
    EXPECT_EQ(VALID_CALL, bidding.getCall(0));
}

TEST_F(BiddingTest, testGetCallOutOfRange)
{
    EXPECT_CALL(bidding, handleGetNumberOfCalls());
    EXPECT_CALL(bidding, handleGetCall(_)).Times(0);
    EXPECT_THROW(bidding.getCall(0), std::out_of_range);
}

TEST_P(BiddingTest, testAllowedCallWhenPlayerHasTurn)
{
    auto n = GetParam();
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillOnce(Return(n));
    EXPECT_CALL(bidding, handleAddCall(VALID_CALL));
    EXPECT_CALL(bidding, handleIsCallAllowed(VALID_CALL));
    EXPECT_TRUE(bidding.call(clockwise(Position::NORTH, n), VALID_CALL));
}

TEST_F(BiddingTest, testAllowedCallWhenPlayerDoesNotHaveTurn)
{
    EXPECT_CALL(bidding, handleHasEnded());
    EXPECT_CALL(bidding, handleIsCallAllowed(_)).Times(0);
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_FALSE(bidding.call(Position::EAST, VALID_CALL));
}

TEST_F(BiddingTest, testNotAllowedCallWhenPlayerHasTurn)
{
    EXPECT_CALL(bidding, handleHasEnded());
    EXPECT_CALL(bidding, handleIsCallAllowed(INVALID_CALL));
    EXPECT_CALL(bidding, handleAddCall(_)).Times(0);
    EXPECT_FALSE(bidding.call(Position::NORTH, INVALID_CALL));
}

TEST_F(BiddingTest, testCallWhenBiddingHasEnded)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleIsCallAllowed(_)).Times(0);
    EXPECT_CALL(bidding, handleAddCall(INVALID_CALL)).Times(0);
    EXPECT_FALSE(bidding.call(Position::NORTH, VALID_CALL));
}

TEST_P(BiddingTest, testPlayerInTurnWhenBiddingIsOngoing)
{
    const auto n = GetParam();
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillOnce(Return(n));
    EXPECT_EQ(clockwise(Position::NORTH, n), bidding.getPositionInTurn());
}

TEST_F(BiddingTest, testPlayerInTurnWhenBiddingHasEnded)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_FALSE(bidding.getPositionInTurn());
}

TEST_F(BiddingTest, testHasEnded)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_TRUE(bidding.hasEnded());
}

TEST_F(BiddingTest, testHasNotEnded)
{
    EXPECT_CALL(bidding, handleHasEnded());
    EXPECT_FALSE(bidding.hasEnded());
}

TEST_F(BiddingTest, testHasContractWhenBiddingIsOngoing)
{
    EXPECT_CALL(bidding, handleHasEnded());
    EXPECT_TRUE(indeterminate(bidding.hasContract()));
}

TEST_F(BiddingTest, testGetContractWhenBiddingIsOngoing)
{
    EXPECT_CALL(bidding, handleHasEnded());
    EXPECT_CALL(bidding, handleGetContract()).Times(0);
    EXPECT_FALSE(bidding.getContract());
}

TEST_F(BiddingTest, testGetDeclarerWhenBiddingIsOngoing)
{
    EXPECT_CALL(bidding, handleHasEnded());
    EXPECT_CALL(bidding, handleGetDeclarerPosition()).Times(0);
    EXPECT_FALSE(bidding.getDeclarerPosition());
}

TEST_F(BiddingTest, testHasContractWhenBiddingHasEndedAndThereIsContract)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleHasContract());
    EXPECT_TRUE(bidding.hasContract());
}

TEST_F(BiddingTest, testGetContractWhenBiddingHasEndedAndThereIsContract)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleHasContract());
    EXPECT_CALL(bidding, handleGetContract());
    EXPECT_EQ(boost::make_optional(CONTRACT), bidding.getContract());
}

TEST_F(BiddingTest, testGetDeclarerWhenBiddingHasEndedAndThereIsContract)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleHasContract());
    EXPECT_CALL(bidding, handleGetDeclarerPosition());
    const auto declarer = bidding.getDeclarerPosition();
    ASSERT_TRUE(declarer);
    EXPECT_EQ(Position::NORTH, *declarer);
}

TEST_F(BiddingTest, testHasContractWhenBiddingHasEndedAndThereIsNoContract)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleHasContract()).WillOnce(Return(false));
    EXPECT_FALSE(bidding.hasContract());
}

TEST_F(BiddingTest, testGetContractWhenBiddingHasEndedAndThereIsNoContract)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleHasContract()).WillOnce(Return(false));
    EXPECT_CALL(bidding, handleGetContract()).Times(0);
    EXPECT_EQ(boost::make_optional(boost::optional<Contract>(boost::none)),
              bidding.getContract());
}

TEST_F(BiddingTest, testGetDeclarerWhenBiddingHasEndedAndThereIsNoContract)
{
    EXPECT_CALL(bidding, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(bidding, handleHasContract()).WillOnce(Return(false));
    EXPECT_CALL(bidding, handleGetDeclarerPosition()).Times(0);
    const auto declarer = bidding.getDeclarerPosition();
    ASSERT_TRUE(declarer);
    EXPECT_FALSE(*declarer);
}

INSTANTIATE_TEST_CASE_P(
    SamplingRounds, BiddingTest, ValuesIn(to(2 * Bridge::N_PLAYERS)));
