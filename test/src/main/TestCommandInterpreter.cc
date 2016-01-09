#include "bridge/Bid.hh"
#include "bridge/Call.hh"
#include "bridge/CardType.hh"
#include "main/CommandInterpreter.hh"
#include "MockBridgeController.hh"

#include <gtest/gtest.h>

class CommandInterpreterTest : public testing::Test
{
protected:
    testing::StrictMock<Bridge::MockBridgeController> controller;
    Bridge::Main::CommandInterpreter interpreter {controller};
};

TEST_F(CommandInterpreterTest, testPass)
{
    EXPECT_CALL(controller, handleCall(Bridge::Call {Bridge::Pass {}}));
    EXPECT_TRUE(interpreter.interpret("call pass"));
}

TEST_F(CommandInterpreterTest, testDouble)
{
    EXPECT_CALL(controller, handleCall(Bridge::Call {Bridge::Double {}}));
    EXPECT_TRUE(interpreter.interpret("call double"));
}

TEST_F(CommandInterpreterTest, testRedouble)
{
    EXPECT_CALL(controller, handleCall(Bridge::Call {Bridge::Redouble {}}));
    EXPECT_TRUE(interpreter.interpret("call redouble"));
}

TEST_F(CommandInterpreterTest, testBid)
{
    constexpr Bridge::Bid bid {1, Bridge::Strain::CLUBS};
    EXPECT_CALL(controller, handleCall(Bridge::Call {bid}));
    EXPECT_TRUE(interpreter.interpret("call bid 1 clubs"));
}

TEST_F(CommandInterpreterTest, testEmptyBid)
{
    EXPECT_FALSE(interpreter.interpret("call bid"));
}

TEST_F(CommandInterpreterTest, testBidWithEmptyStrain)
{
    EXPECT_FALSE(interpreter.interpret("call bid 1"));
}

TEST_F(CommandInterpreterTest, testBidWithInvalidStrain)
{
    EXPECT_FALSE(interpreter.interpret("call bid 1 invalid"));
}

TEST_F(CommandInterpreterTest, testBidWithInvalidLevel)
{
    EXPECT_FALSE(interpreter.interpret("call bid 8 clubs"));
}

TEST_F(CommandInterpreterTest, testInvalidCall)
{
    EXPECT_FALSE(interpreter.interpret("call invalid"));
}

TEST_F(CommandInterpreterTest, testInvalidCommand)
{
    EXPECT_FALSE(interpreter.interpret("invalid"));
}

TEST_F(CommandInterpreterTest, testPlay)
{
    EXPECT_CALL(controller, handlePlay(
                    Bridge::CardType {
                        Bridge::Rank::ACE, Bridge::Suit::SPADES}));
    EXPECT_TRUE(interpreter.interpret("play ace spades"));
}

TEST_F(CommandInterpreterTest, testPlayInvalidSuit)
{
    EXPECT_FALSE(interpreter.interpret("play ace invalid"));
}

TEST_F(CommandInterpreterTest, testPlayInvalidRank)
{
    EXPECT_FALSE(interpreter.interpret("play invalid spade"));
}

TEST_F(CommandInterpreterTest, testPlayEmpty)
{
    EXPECT_FALSE(interpreter.interpret("play"));
}
