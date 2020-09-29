#include "main/NodePlayerControl.hh"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>

namespace {
const auto NODE = Bridge::Messaging::UserId {"node"};
const auto OTHER_NODE = Bridge::Messaging::UserId {"other"};
boost::uuids::string_generator STRING_GENERATOR;
const auto UUID = STRING_GENERATOR("a3cc5805-544f-415b-ba86-31f6237bf122");
const auto OTHER_UUID = STRING_GENERATOR("5913e360-0a82-44fe-8406-e486b3a9f8bb");
}

class NodePlayerControlTest : public testing::Test
{
protected:
    Bridge::Main::NodePlayerControl nodePlayerControl;
};

TEST_F(NodePlayerControlTest, testCreatePlayer)
{
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, UUID, nullptr);
    ASSERT_TRUE(player);
    EXPECT_EQ(UUID, player->getUuid());
}

TEST_F(NodePlayerControlTest, testCreatePlayerWithSameUuidAndNode)
{
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, UUID, nullptr);
    const auto player2 = nodePlayerControl.getOrCreatePlayer(NODE, UUID, nullptr);
    EXPECT_EQ(player, player2);
}

TEST_F(NodePlayerControlTest, testCreateMultiplePlayers)
{
    nodePlayerControl.getOrCreatePlayer(NODE, UUID, nullptr);
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, OTHER_UUID, nullptr);
    ASSERT_TRUE(player);
    EXPECT_EQ(OTHER_UUID, player->getUuid());
}

TEST_F(NodePlayerControlTest, testCreatePlayerForMultipleNodes)
{
    nodePlayerControl.getOrCreatePlayer(NODE, UUID, nullptr);
    const auto player = nodePlayerControl.getOrCreatePlayer(OTHER_NODE, OTHER_UUID, nullptr);
    ASSERT_TRUE(player);
    EXPECT_EQ(OTHER_UUID, player->getUuid());
}

TEST_F(NodePlayerControlTest, testCreatePlayerForOtherNodeWithConflictingUuid)
{
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, UUID, nullptr);
    EXPECT_FALSE(nodePlayerControl.getOrCreatePlayer(OTHER_NODE, UUID, nullptr));
}
