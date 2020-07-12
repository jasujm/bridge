#include "bridge/Deal.hh"
#include "bridge/Position.hh"
#include "Enumerate.hh"
#include "MockBidding.hh"
#include "MockDeal.hh"
#include "MockHand.hh"
#include "MockTrick.hh"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>

#include <memory>
#include <map>
#include <stdexcept>

namespace Positions = Bridge::Positions;
using Bridge::DealPhase;
using Bridge::MockHand;
using Bridge::MockTrick;
using Bridge::Position;
using testing::_;
using testing::AtLeast;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnPointee;

class DealTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (const auto position : Position::all()) {
            ON_CALL(deal, handleGetHand(position))
                .WillByDefault(ReturnPointee(hands.at(position)));
        }
        for (const auto [n, trick_ptr] : Bridge::enumerate(trick_ptrs)) {
            ON_CALL(deal, handleGetTrick(n)).WillByDefault(Return(tricks.at(n)));
        }
        ON_CALL(deal, handleGetNumberOfTricks()).WillByDefault(Return(tricks.size()));
        ON_CALL(deal, handleGetBidding()).WillByDefault(ReturnRef(bidding));
    }

    void configureBiddingPhase(const Position openerPosition)
    {
        ON_CALL(deal, handleGetPhase()).WillByDefault(Return(DealPhase::BIDDING));
        ON_CALL(bidding, handleHasEnded()).WillByDefault(Return(false));
        ON_CALL(bidding, handleGetOpeningPosition())
            .WillByDefault(Return(openerPosition));
    }

    void configurePlayingPhase(const Position declarerPosition)
    {
        ON_CALL(deal, handleGetPhase()).WillByDefault(Return(DealPhase::PLAYING));
        ON_CALL(bidding, handleHasEnded()).WillByDefault(Return(true));
        ON_CALL(bidding, handleHasContract()).WillByDefault(Return(true));
        ON_CALL(bidding, handleGetDeclarerPosition())
            .WillByDefault(Return(declarerPosition));
    }

    void configureEndedPhase()
    {
        ON_CALL(deal, handleGetPhase()).WillByDefault(Return(DealPhase::ENDED));
    }

    std::map<Position, std::shared_ptr<Bridge::MockHand>> hands {
        { Positions::NORTH, std::make_shared<Bridge::MockHand>() },
        { Positions::EAST,  std::make_shared<Bridge::MockHand>() },
        { Positions::SOUTH, std::make_shared<Bridge::MockHand>() },
        { Positions::WEST,  std::make_shared<Bridge::MockHand>() },
    };
    Bridge::MockBidding bidding;
    std::vector<std::shared_ptr<MockTrick>> trick_ptrs {
        std::make_shared<Bridge::MockTrick>(),
        std::make_shared<Bridge::MockTrick>(),
    };
    std::vector<Bridge::Deal::TrickPositionPair> tricks {
        { std::cref(*trick_ptrs.front()), Positions::WEST },
        { std::cref(*trick_ptrs.back()),  std::nullopt },
    };
    testing::NiceMock<Bridge::MockDeal> deal;
};

TEST_F(DealTest, testUuid)
{
    boost::uuids::string_generator gen;
    const auto UUID = gen("45c49107-6f1b-41be-9441-5c46a65bdbed");
    EXPECT_CALL(deal, handleGetUuid()).WillOnce(ReturnRef(UUID));
    EXPECT_EQ(UUID, deal.getUuid());
}

TEST_F(DealTest, testVulnerability)
{
    constexpr auto VULNERABILITY = Bridge::Vulnerability {true, false};
    EXPECT_CALL(deal, handleGetVulnerability()).WillOnce(Return(VULNERABILITY));
    EXPECT_EQ(VULNERABILITY, deal.getVulnerability());
}

TEST_F(DealTest, testPositionInTurnDuringBidding)
{
    configureBiddingPhase(Positions::EAST);
    EXPECT_CALL(bidding, handleGetNumberOfCalls()).WillOnce(Return(0));
    EXPECT_EQ(Positions::EAST, deal.getPositionInTurn());
}

TEST_F(DealTest, testPositionInTurnDuringPlaying)
{
    configurePlayingPhase(Positions::NORTH);
    EXPECT_CALL(*trick_ptrs.back(), handleGetNumberOfCardsPlayed())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*trick_ptrs.back(), handleGetHand(0))
        .WillOnce(ReturnRef(*hands.at(Positions::NORTH)))
        .WillOnce(ReturnRef(*hands.at(Positions::EAST)))
        .WillOnce(ReturnRef(*hands.at(Positions::SOUTH)))
        .WillOnce(ReturnRef(*hands.at(Positions::WEST)));
    for (const auto position : Position::all()) {
        // North is declarer, thus takes turn both for north and south
        const auto expected_position = (position == Positions::SOUTH) ?
            Positions::NORTH : position;
        EXPECT_EQ(expected_position, deal.getPositionInTurn());
    }
}

TEST_F(DealTest, testPositionInTurnDealEnded)
{
    configureEndedPhase();
    EXPECT_EQ(std::nullopt, deal.getPositionInTurn());
}

TEST_F(DealTest, testHandInTurn)
{
    configurePlayingPhase(Positions::NORTH);
    EXPECT_CALL(*trick_ptrs.back(), handleGetNumberOfCardsPlayed())
        .WillOnce(Return(0));
    EXPECT_CALL(*trick_ptrs.back(), handleGetHand(0))
        .WillOnce(ReturnRef(*hands.at(Positions::NORTH)));
    EXPECT_EQ(hands.at(Positions::NORTH).get(), deal.getHandInTurn());
}

TEST_F(DealTest, testHandInTurnIfNotPlaying)
{
    configureBiddingPhase(Positions::EAST);
    EXPECT_EQ(nullptr, deal.getHandInTurn());
}

TEST_F(DealTest, testGetHand)
{
    constexpr auto position = Positions::EAST;
    EXPECT_CALL(deal, handleGetHand(position));
    EXPECT_EQ(hands.at(position).get(), &deal.getHand(position));
}

TEST_F(DealTest, testGetPosition)
{
    constexpr auto position = Positions::SOUTH;
    EXPECT_CALL(deal, handleGetHand(_)).Times(AtLeast(1));
    EXPECT_EQ(position, deal.getPosition(*hands.at(position)));
}

TEST_F(DealTest, testGetPositionIfHandIsNotInTheGame)
{
    const auto hand = Bridge::MockHand {};
    EXPECT_CALL(deal, handleGetHand(_)).Times(AtLeast(1));
    EXPECT_EQ(std::nullopt, deal.getPosition(hand));
}

TEST_F(DealTest, testVisibleToAllDuringBidding)
{
    configureBiddingPhase(Positions::EAST);
    for (const auto position : Position::all()) {
        EXPECT_FALSE(deal.isVisibleToAll(position));
    }
}

TEST_F(DealTest, testDummyIsNotVisibleToAllBeforeOpeningLead)
{
    configurePlayingPhase(Positions::NORTH);
    EXPECT_CALL(*trick_ptrs.front(), handleGetNumberOfCardsPlayed())
        .WillOnce(Return(0));
    EXPECT_FALSE(deal.isVisibleToAll(Positions::SOUTH));
}

TEST_F(DealTest, testDummyIsVisibleToAllAfterOpeningLead)
{
    configurePlayingPhase(Positions::NORTH);
    EXPECT_CALL(*trick_ptrs.front(), handleGetNumberOfCardsPlayed())
        .WillRepeatedly(Return(1));
    for (const auto position : Position::all()) {
        EXPECT_EQ(position == Positions::SOUTH, deal.isVisibleToAll(position));
    }
}

TEST_F(DealTest, testVisibleToAllAfterDeal)
{
    configureEndedPhase();
    for (const auto position : Position::all()) {
        EXPECT_TRUE(deal.isVisibleToAll(position));
    }
}

TEST_F(DealTest, testBidding)
{
    EXPECT_CALL(deal, handleGetBidding());
    EXPECT_EQ(&bidding, &deal.getBidding());
}

TEST_F(DealTest, testNumberOfTricks)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks());
    EXPECT_EQ(tricks.size(), deal.getNumberOfTricks());
}

TEST_F(DealTest, testGetTrick)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks());
    EXPECT_CALL(deal, handleGetTrick(0));
    const auto& [expected_trick, expected_winner] = tricks.front();
    const auto& [trick, winner] = deal.getTrick(0);
    EXPECT_EQ(&expected_trick.get(), &trick.get());
    EXPECT_EQ(expected_winner, winner);
}

TEST_F(DealTest, testGetTrickOutOfRange)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks());
    EXPECT_THROW(deal.getTrick(tricks.size()), std::out_of_range);
}

TEST_F(DealTest, testGetCurrentTrick)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks());
    EXPECT_CALL(deal, handleGetTrick(tricks.size() - 1));
    EXPECT_EQ(trick_ptrs.back().get(), deal.getCurrentTrick());
}

TEST_F(DealTest, testGetCurrentTrickOutsidePlayingPhase)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks()).WillOnce(Return(0));
    EXPECT_EQ(nullptr, deal.getCurrentTrick());
}
