#include "bridge/BridgeConstants.hh"
#include "bridge/Card.hh"
#include "bridge/CardType.hh"
#include "bridge/CardTypeIterator.hh"
#include "bridge/Position.hh"
#include "main/PeerlessCardProtocol.hh"
#include "Utility.hh"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <ranges>
#include <vector>

using Bridge::cardTypeIterator;
using Bridge::dereference;
using Bridge::N_CARDS;

class PeerlessCardProtocolTest : public testing::Test {
protected:
    Bridge::Main::PeerlessCardProtocol protocol;
};

TEST_F(PeerlessCardProtocolTest, testItShouldNotAcceptPeers)
{
    EXPECT_FALSE(
        protocol.acceptPeer(
            Bridge::Messaging::Identity(),
            Bridge::Main::CardProtocol::PositionVector(),
            std::nullopt));
}

TEST_F(PeerlessCardProtocolTest, testItShouldNotHaveMessageHandler)
{
    EXPECT_EQ(nullptr, protocol.getDealMessageHandler());
}

TEST_F(PeerlessCardProtocolTest, testItShouldNotHaveSockets)
{
    const auto sockets = protocol.getSockets();
    EXPECT_TRUE(sockets.empty());
}

TEST_F(PeerlessCardProtocolTest, testCardProtocol)
{
    protocol.initialize();
    const auto manager = protocol.getCardManager();
    ASSERT_NE(nullptr, manager);
    EXPECT_FALSE(manager->isShuffleCompleted());
    manager->requestShuffle();
    EXPECT_EQ(N_CARDS, manager->getNumberOfCards());
    const auto cards = Bridge::to(N_CARDS) |
        std::ranges::views::transform(
            [&manager](const auto n)
            {
                return dereference(dereference(manager->getCard(n)).getType());
            });
    EXPECT_TRUE(
        std::is_permutation(
            cardTypeIterator(0), cardTypeIterator(N_CARDS),
            cards.begin(), cards.end()));
    EXPECT_FALSE(
        std::equal(
            cardTypeIterator(0), cardTypeIterator(N_CARDS),
            cards.begin(), cards.end()));
}
