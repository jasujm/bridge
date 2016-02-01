#include "bridge/Bid.hh"
#include "bridge/Contract.hh"
#include "bridge/Partnership.hh"
#include "bridge/Position.hh"
#include "bridge/Vulnerability.hh"
#include "MockGameManager.hh"

#include <gtest/gtest.h>

using Bridge::Position;
using Bridge::Vulnerability;

using testing::_;
using testing::Return;

namespace {
constexpr Bridge::Partnership PARTNERSHIP {Bridge::Partnership::NORTH_SOUTH};
const Bridge::Contract CONTRACT {Bridge::Bid {2, Bridge::Strain::CLUBS},
                                 Bridge::Doubling::UNDOUBLED};
constexpr int TRICKS_WON {7};
}

class GameManagerTest : public testing::Test {
protected:
    Bridge::Engine::MockGameManager gameManager;
};

TEST_F(GameManagerTest, testAddResult)
{
    EXPECT_CALL(gameManager, handleHasEnded());
    EXPECT_CALL(
        gameManager, handleAddResult(PARTNERSHIP, CONTRACT, TRICKS_WON));
    gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
}

TEST_F(GameManagerTest, testAddResultWhenGameHasEnded)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(gameManager, handleAddResult(_, _, _)).Times(0);
    gameManager.addResult(PARTNERSHIP, CONTRACT, TRICKS_WON);
}

TEST_F(GameManagerTest, testPassedOut)
{
    EXPECT_CALL(gameManager, handleHasEnded());
    EXPECT_CALL(gameManager, handleAddPassedOut());
    gameManager.addPassedOut();
}

TEST_F(GameManagerTest, testPassedOutWhenGameHasEnded)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(gameManager, handleAddPassedOut()).Times(0);
    gameManager.addPassedOut();
}

TEST_F(GameManagerTest, testGameNotEnded)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(false));
    EXPECT_FALSE(gameManager.hasEnded());
}

TEST_F(GameManagerTest, testGameEnded)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(true));
    EXPECT_TRUE(gameManager.hasEnded());
}

TEST_F(GameManagerTest, testGetOpenerPosition)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(false));
    EXPECT_CALL(gameManager, handleGetOpenerPosition()).WillOnce(
        Return(Position::NORTH));
    EXPECT_EQ(Position::NORTH, gameManager.getOpenerPosition());
}

TEST_F(GameManagerTest, testGetOpenerPositionWhenGameHasEnded)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(gameManager, handleGetOpenerPosition()).Times(0);
    EXPECT_FALSE(gameManager.getOpenerPosition());
}

TEST_F(GameManagerTest, testGetVulnerability)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(false));
    EXPECT_CALL(gameManager, handleGetVulnerability()).WillOnce(
        Return(Vulnerability {true, true}));
    EXPECT_EQ(Vulnerability(true, true), gameManager.getVulnerability());
}

TEST_F(GameManagerTest, testGetVulnerabilityWhenGameHasEnded)
{
    EXPECT_CALL(gameManager, handleHasEnded()).WillOnce(Return(true));
    EXPECT_CALL(gameManager, handleGetVulnerability()).Times(0);
    EXPECT_FALSE(gameManager.getVulnerability());
}
