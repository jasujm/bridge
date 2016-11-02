#include "bridge/BasicPlayer.hh"
#include "bridge/BridgeConstants.hh"
#include "main/NodeControl.hh"

#include <gtest/gtest.h>

#include <array>
#include <iterator>

namespace {

using namespace std::string_literals;

const auto CLIENT1 = "client1"s;
const auto CLIENT2 = "client2"s;
const auto PEER1 = "peer1"s;
const auto PEER2 = "peer2"s;

}

class NodeControlTest : public testing::Test {
protected:
    void addClients()
    {
        nodeControl.addClient(CLIENT1);
        nodeControl.addClient(CLIENT2);
    }

    void addPeer()
    {
        nodeControl.addPeer(
            PEER1, std::next(players.begin(), 2), players.end());
    }

    std::array<Bridge::BasicPlayer, Bridge::N_PLAYERS> players;
    Bridge::Main::NodeControl nodeControl {
        players.begin(), std::next(players.begin(), 2)};
};

TEST_F(NodeControlTest, testAddClient)
{
    EXPECT_EQ(&players[0], nodeControl.addClient(CLIENT1));
    EXPECT_EQ(&players[1], nodeControl.addClient(CLIENT2));
}

TEST_F(NodeControlTest, testAddExistingClient)
{
    const auto player = nodeControl.addClient(CLIENT1);
    EXPECT_EQ(player, nodeControl.addClient(CLIENT1));
}

TEST_F(
    NodeControlTest, testOnlyOneClientClientCanBeAddedPerPlayerRepresented)
{
    addClients();
    EXPECT_FALSE(nodeControl.addClient("extra_client"));
}

TEST_F(NodeControlTest, testClientIsAllowedToActForTheirPlayer)
{
    addClients();
    EXPECT_TRUE(nodeControl.isAllowedToAct(CLIENT1, players[0]));
    EXPECT_TRUE(nodeControl.isAllowedToAct(CLIENT2, players[1]));
}

TEST_F(NodeControlTest, testClientIsNotAllowedToActForOtherPlayer)
{
    addClients();
    EXPECT_FALSE(nodeControl.isAllowedToAct(CLIENT1, players[1]));
    EXPECT_FALSE(nodeControl.isAllowedToAct(CLIENT2, players[0]));
}

TEST_F(NodeControlTest, testAddPeer)
{
    EXPECT_TRUE(
        nodeControl.addPeer(
            PEER1, std::next(players.begin(), 2), players.end()));
}

TEST_F(NodeControlTest, testPeerCannotRepresentPlayersRepresentedBySelf)
{
    addPeer();
    EXPECT_FALSE(
        nodeControl.addPeer(
            PEER1, players.begin(), players.end()));
}

TEST_F(NodeControlTest, testPeerCannotRepresentPlayersRepresentedByOtherPeer)
{
    addPeer();
    EXPECT_FALSE(
        nodeControl.addPeer(
            PEER2, std::next(players.begin(), 2), players.end()));
}

TEST_F(NodeControlTest, testTwoPeersCannotHaveSameIdentity)
{
    nodeControl.addPeer(
        PEER1, std::next(players.begin(), 2), std::next(players.begin(), 3));
    EXPECT_FALSE(
        nodeControl.addPeer(
            PEER1, std::next(players.begin(), 3), players.end()));
}

TEST_F(NodeControlTest, testGetPlayerUnrecognizedIdentity)
{
    EXPECT_FALSE(nodeControl.getPlayer(CLIENT1));
}

TEST_F(NodeControlTest, testGetPlayerClient)
{
    addClients();
    EXPECT_EQ(&players[0], nodeControl.getPlayer(CLIENT1));
    EXPECT_EQ(&players[1], nodeControl.getPlayer(CLIENT2));
}

TEST_F(NodeControlTest, testGetPlayerPeerWithSinglePlayer)
{
    nodeControl.addPeer(
        PEER1, std::next(players.begin(), 3), players.end());
    EXPECT_EQ(&players[3], nodeControl.getPlayer(PEER1));
}

TEST_F(NodeControlTest, testGetPlayerPeerWithMultiplerPlayers)
{
    addPeer();
    EXPECT_FALSE(nodeControl.getPlayer(PEER1));
}

TEST_F(NodeControlTest, testPeerIsAllowedToActForPlayersItControls)
{
    addPeer();
    EXPECT_TRUE(nodeControl.isAllowedToAct(PEER1, players[2]));
    EXPECT_TRUE(nodeControl.isAllowedToAct(PEER1, players[3]));
}

TEST_F(
    NodeControlTest, testPeerIsNotAllowedToActForPlayersItDoesNotControl)
{
    addPeer();
    EXPECT_FALSE(nodeControl.isAllowedToAct(PEER1, players[0]));
    EXPECT_FALSE(nodeControl.isAllowedToAct(PEER1, players[1]));
}

TEST_F(NodeControlTest, testPlayersBelongingToSelf)
{
    EXPECT_TRUE(nodeControl.isSelfRepresentedPlayer(players[0]));
    EXPECT_TRUE(nodeControl.isSelfRepresentedPlayer(players[1]));
}

TEST_F(NodeControlTest, testPlayersNotBelongingToSelf)
{
    EXPECT_FALSE(nodeControl.isSelfRepresentedPlayer(players[2]));
    EXPECT_FALSE(nodeControl.isSelfRepresentedPlayer(players[3]));
}

TEST_F(NodeControlTest, testAllPlayersNotRepresented)
{
    EXPECT_FALSE(
        nodeControl.arePlayersRepresented(players.begin(), players.end()));
}

TEST_F(NodeControlTest, testAllPlayersRepresented)
{
    addPeer();
    EXPECT_TRUE(
        nodeControl.arePlayersRepresented(players.begin(), players.end()));
}
