#include "bridge/Bid.hh"
#include "bridge/CardType.hh"
#include "bridge/Contract.hh"
#include "bridge/Deal.hh"
#include "bridge/Position.hh"
#include "bridge/SimpleCard.hh"
#include "bridge/TricksWon.hh"
#include "Enumerate.hh"
#include "MockBidding.hh"
#include "MockDeal.hh"
#include "MockHand.hh"
#include "MockTrick.hh"
#include "Utility.hh"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>

#include <memory>
#include <map>
#include <stdexcept>

namespace Positions = Bridge::Positions;
namespace Ranks = Bridge::Ranks;
namespace Suits = Bridge::Suits;
using Bridge::CardType;
using Bridge::DealPhase;
using Bridge::MockHand;
using Bridge::MockTrick;
using Bridge::Position;
using Bridge::SimpleCard;
using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnPointee;

namespace {

const auto CARDS_IN_EXAMPLE_TRICK = std::array {
    SimpleCard {CardType {Ranks::QUEEN, Suits::SPADES}},
    SimpleCard {CardType {Ranks::ACE, Suits::SPADES}},
    SimpleCard {CardType {Ranks::TWO, Suits::CLUBS}},
    SimpleCard {CardType {Ranks::SEVEN, Suits::DIAMONDS}},
};

}

class DealTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        for (const auto position : Position::all()) {
            ON_CALL(deal, handleGetHand(position))
                .WillByDefault(ReturnPointee(hands.at(position)));
        }
        for (const auto n : Bridge::to(ssize(tricks))) {
            ON_CALL(deal, handleGetTrick(n)).WillByDefault(ReturnPointee(tricks.at(n)));
            for (const auto [m, position] : enumerate(Position::all())) {
                ON_CALL(*tricks.at(0), handleGetHand(m))
                    .WillByDefault(ReturnPointee(hands.at(position)));
            }
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

    void configureExampleTrick()
    {
        configurePlayingPhase(Positions::NORTH);
        ON_CALL(bidding, handleGetContract())
            .WillByDefault(
                Return(
                    Bridge::Contract {
                        Bridge::Bid {1, Bridge::Strains::CLUBS},
                        Bridge::Doublings::UNDOUBLED}));
        ON_CALL(*tricks.front(), handleGetNumberOfCardsPlayed())
            .WillByDefault(Return(ssize(CARDS_IN_EXAMPLE_TRICK)));
        for (const auto n : Bridge::to(ssize(CARDS_IN_EXAMPLE_TRICK))) {
            ON_CALL(*tricks.front(), handleGetCard(n))
                .WillByDefault(ReturnRef(CARDS_IN_EXAMPLE_TRICK.at(n)));
        }
    }

    void configureEndedPhase()
    {
        ON_CALL(deal, handleGetPhase()).WillByDefault(Return(DealPhase::ENDED));
    }

    std::map<Position, std::shared_ptr<Bridge::MockHand>> hands {
        { Positions::NORTH, std::make_shared<NiceMock<Bridge::MockHand>>() },
        { Positions::EAST,  std::make_shared<NiceMock<Bridge::MockHand>>() },
        { Positions::SOUTH, std::make_shared<NiceMock<Bridge::MockHand>>() },
        { Positions::WEST,  std::make_shared<NiceMock<Bridge::MockHand>>() },
    };
    NiceMock<Bridge::MockBidding> bidding;
    std::vector<std::shared_ptr<MockTrick>> tricks {
        std::make_shared<NiceMock<Bridge::MockTrick>>(),
        std::make_shared<NiceMock<Bridge::MockTrick>>(),
    };
    NiceMock<Bridge::MockDeal> deal;
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
    EXPECT_CALL(*tricks.back(), handleGetNumberOfCardsPlayed())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*tricks.back(), handleGetHand(0))
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
    EXPECT_CALL(*tricks.back(), handleGetNumberOfCardsPlayed())
        .WillOnce(Return(0));
    EXPECT_CALL(*tricks.back(), handleGetHand(0))
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
    EXPECT_CALL(*tricks.front(), handleGetNumberOfCardsPlayed())
        .WillOnce(Return(0));
    EXPECT_FALSE(deal.isVisibleToAll(Positions::SOUTH));
}

TEST_F(DealTest, testDummyIsVisibleToAllAfterOpeningLead)
{
    configurePlayingPhase(Positions::NORTH);
    EXPECT_CALL(*tricks.front(), handleGetNumberOfCardsPlayed())
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
    EXPECT_EQ(tricks.front().get(), &deal.getTrick(0));
}

TEST_F(DealTest, testGetWinnerOfTrick)
{
    configureExampleTrick();
    EXPECT_EQ(Positions::SOUTH, deal.getWinnerOfTrick(0));
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
    EXPECT_EQ(tricks.back().get(), deal.getCurrentTrick());
}

TEST_F(DealTest, testGetCurrentTrickOutsidePlayingPhase)
{
    EXPECT_CALL(deal, handleGetNumberOfTricks()).WillOnce(Return(0));
    EXPECT_EQ(nullptr, deal.getCurrentTrick());
}

TEST_F(DealTest, testTricksWon)
{
    configureExampleTrick();
    EXPECT_EQ(Bridge::TricksWon(1, 0), deal.getTricksWon());
}
