#include "MockPlayer.hh"

#include <boost/uuid/string_generator.hpp>

using testing::Return;

class PlayerTest : public testing::Test {
protected:
    Bridge::MockPlayer player;
};

TEST_F(PlayerTest, testGetUuid)
{
    const auto uuid_string = "a3cc5805-544f-415b-ba86-31f6237bf122";
    boost::uuids::string_generator gen;
    const auto uuid = gen(uuid_string);
    EXPECT_CALL(player, handleGetUuid()).WillOnce(Return(uuid));
    EXPECT_EQ(uuid, player.getUuid());
}
