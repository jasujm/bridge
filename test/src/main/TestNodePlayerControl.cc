#include "main/NodePlayerControl.hh"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>

namespace {
using namespace std::string_literals;
using namespace Bridge::BlobLiterals;
const auto NODE = Bridge::Messaging::Identity { ""s, "node"_B };
const auto OTHER_NODE = Bridge::Messaging::Identity { ""s, "other"_B };
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
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, UUID);
    ASSERT_TRUE(player);
    EXPECT_EQ(UUID, player->getUuid());
}

TEST_F(NodePlayerControlTest, testCreatePlayerWithSameUuidAndNode)
{
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, UUID);
    const auto player2 = nodePlayerControl.getOrCreatePlayer(NODE, UUID);
    EXPECT_EQ(player, player2);
}

TEST_F(NodePlayerControlTest, testCreateMultiplePlayers)
{
    nodePlayerControl.getOrCreatePlayer(NODE, UUID);
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, OTHER_UUID);
    ASSERT_TRUE(player);
    EXPECT_EQ(OTHER_UUID, player->getUuid());
}

TEST_F(NodePlayerControlTest, testCreatePlayerForMultipleNodes)
{
    nodePlayerControl.getOrCreatePlayer(NODE, UUID);
    const auto player = nodePlayerControl.getOrCreatePlayer(OTHER_NODE, OTHER_UUID);
    ASSERT_TRUE(player);
    EXPECT_EQ(OTHER_UUID, player->getUuid());
}

// Disabled due to NodePlayerControl not supporting authentication except in
// CURVE mechanism
TEST_F(NodePlayerControlTest, DISABLED_testCreatePlayerForOtherNodeWithConflictingUuid)
{
    const auto player = nodePlayerControl.getOrCreatePlayer(NODE, UUID);
    EXPECT_FALSE(nodePlayerControl.getOrCreatePlayer(OTHER_NODE, UUID));
}
