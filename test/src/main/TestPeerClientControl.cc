#include "bridge/BasicPlayer.hh"
#include "bridge/BridgeConstants.hh"
#include "main/PeerClientControl.hh"

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

class PeerClientControlTest : public testing::Test {
protected:
    void addClients()
    {
        peerClientControl.addClient(CLIENT1);
        peerClientControl.addClient(CLIENT2);
    }

    void addPeer()
    {
        peerClientControl.addPeer(
            PEER1, std::next(players.begin(), 2), players.end());
    }

    std::array<Bridge::BasicPlayer, Bridge::N_PLAYERS> players;
    Bridge::Main::PeerClientControl peerClientControl {
        players.begin(), std::next(players.begin(), 2)};
};

TEST_F(PeerClientControlTest, testAddClient)
{
    EXPECT_EQ(&players[0], peerClientControl.addClient(CLIENT1));
    EXPECT_EQ(&players[1], peerClientControl.addClient(CLIENT2));
}

TEST_F(PeerClientControlTest, testTwoClientsCannotHaveSameIdentity)
{
    peerClientControl.addClient(CLIENT1);
    EXPECT_FALSE(peerClientControl.addClient(CLIENT1));
}

TEST_F(
    PeerClientControlTest, testOnlyOneClientClientCanBeAddedPerPlayerControlled)
{
    addClients();
    EXPECT_FALSE(peerClientControl.addClient("extra_client"));
}

TEST_F(PeerClientControlTest, testClientIsAllowedToActForTheirPlayer)
{
    addClients();
    EXPECT_TRUE(peerClientControl.isAllowedToAct(CLIENT1, players[0]));
    EXPECT_TRUE(peerClientControl.isAllowedToAct(CLIENT2, players[1]));
}

TEST_F(PeerClientControlTest, testClientIsNotAllowedToActForOtherPlayer)
{
    addClients();
    EXPECT_FALSE(peerClientControl.isAllowedToAct(CLIENT1, players[1]));
    EXPECT_FALSE(peerClientControl.isAllowedToAct(CLIENT2, players[0]));
}

TEST_F(PeerClientControlTest, testAddPeer)
{
    EXPECT_TRUE(
        peerClientControl.addPeer(
            PEER1, std::next(players.begin(), 2), players.end()));
}

TEST_F(PeerClientControlTest, testPeerCannotControlPlayersControlledBySelf)
{
    addPeer();
    EXPECT_FALSE(
        peerClientControl.addPeer(
            PEER1, players.begin(), players.end()));
}

TEST_F(PeerClientControlTest, testPeerCannotControlPlayersControlledByOtherPeer)
{
    addPeer();
    EXPECT_FALSE(
        peerClientControl.addPeer(
            PEER2, std::next(players.begin(), 2), players.end()));
}

TEST_F(PeerClientControlTest, testTwoPeersCannotHaveSameIdentity)
{
    peerClientControl.addPeer(
        PEER1, std::next(players.begin(), 2), std::next(players.begin(), 3));
    EXPECT_FALSE(
        peerClientControl.addPeer(
            PEER1, std::next(players.begin(), 3), players.end()));
}

TEST_F(PeerClientControlTest, testGetPlayerUnrecognizedIdentity)
{
    EXPECT_FALSE(peerClientControl.getPlayer(CLIENT1));
}

TEST_F(PeerClientControlTest, testGetPlayerClient)
{
    addClients();
    EXPECT_EQ(&players[0], peerClientControl.getPlayer(CLIENT1));
    EXPECT_EQ(&players[1], peerClientControl.getPlayer(CLIENT2));
}

TEST_F(PeerClientControlTest, testGetPlayerPeerWithSinglePlayer)
{
    peerClientControl.addPeer(
        PEER1, std::next(players.begin(), 3), players.end());
    EXPECT_EQ(&players[3], peerClientControl.getPlayer(PEER1));
}

TEST_F(PeerClientControlTest, testGetPlayerPeerWithMultiplerPlayers)
{
    addPeer();
    EXPECT_FALSE(peerClientControl.getPlayer(PEER1));
}

TEST_F(PeerClientControlTest, testPeerIsAllowedToActForPlayersItControls)
{
    addPeer();
    EXPECT_TRUE(peerClientControl.isAllowedToAct(PEER1, players[2]));
    EXPECT_TRUE(peerClientControl.isAllowedToAct(PEER1, players[3]));
}

TEST_F(
    PeerClientControlTest, testPeerIsNotAllowedToActForPlayersItDoesNotControl)
{
    addPeer();
    EXPECT_FALSE(peerClientControl.isAllowedToAct(PEER1, players[0]));
    EXPECT_FALSE(peerClientControl.isAllowedToAct(PEER1, players[1]));
}

TEST_F(PeerClientControlTest, testPlayersBelongingToSelf)
{
    EXPECT_TRUE(peerClientControl.isSelfControlledPlayer(players[0]));
    EXPECT_TRUE(peerClientControl.isSelfControlledPlayer(players[1]));
}

TEST_F(PeerClientControlTest, testPlayersNotBelongingToSelf)
{
    EXPECT_FALSE(peerClientControl.isSelfControlledPlayer(players[2]));
    EXPECT_FALSE(peerClientControl.isSelfControlledPlayer(players[3]));
}

TEST_F(PeerClientControlTest, testAllPlayersNotControlled)
{
    EXPECT_FALSE(
        peerClientControl.arePlayersControlled(players.begin(), players.end()));
}

TEST_F(PeerClientControlTest, testAllPlayersControlled)
{
    addPeer();
    EXPECT_TRUE(
        peerClientControl.arePlayersControlled(players.begin(), players.end()));
}
