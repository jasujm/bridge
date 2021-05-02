#include "bridge/Position.hh"
#include "main/BridgeGameConfig.hh"
#include "main/Config.hh"
#include "messaging/EndpointIterator.hh"
#include "messaging/Security.hh"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

using Bridge::Main::BridgeGameConfig;
using Bridge::Main::Config;
using Bridge::Messaging::decodeKey;
using NodeMap = Bridge::Messaging::Authenticator::NodeMap;

class ConfigTest : public testing::Test {
protected:
    std::istringstream in;

    void assertThrows()
    {
        auto f = [this]() { static_cast<void>(Config {in}); };
        EXPECT_THROW(f(), std::runtime_error);
    }
};

TEST_F(ConfigTest, testBadStream)
{
    in.setstate(std::ios::failbit);
    assertThrows();
}

TEST_F(ConfigTest, testBadSyntax)
{
    in.str("this is invalid"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseEndpointConfigMissingEndpoint)
{
    const auto config = Config {in};
    const auto endpoint_iterator = config.getEndpointIterator();
    EXPECT_EQ("tcp://*:5555"s, *endpoint_iterator);
}

TEST_F(ConfigTest, testParseEndpointConfig)
{
    in.str(R"EOF(
bind_address = "localhost"
bind_base_port = 1234
)EOF");
    const auto config = Config {in};
    const auto endpoint_iterator = config.getEndpointIterator();
    EXPECT_EQ("tcp://localhost:1234"s, *endpoint_iterator);
}

TEST_F(ConfigTest, testParseCurveConfigMissingKeys)
{
    const auto config = Config {in};
    const auto curve = config.getCurveConfig();
    EXPECT_FALSE(curve);
}

TEST_F(ConfigTest, testParseCurveConfig)
{
    in.str(R"EOF(
curve_secret_key = "JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6"
curve_public_key = "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"
)EOF"s);
    const auto config = Config {in};
    const auto curve = config.getCurveConfig();
    ASSERT_TRUE(curve);
    const auto expectedSecretKey =
        decodeKey("JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6");
    const auto expectedPublicKey =
        decodeKey("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7");
    EXPECT_EQ(expectedSecretKey, curve->secretKey);
    EXPECT_EQ(expectedPublicKey, curve->publicKey);
}

TEST_F(ConfigTest, testParseDataDirMissingKey)
{
    const auto config = Config {in};
    EXPECT_FALSE(config.getDataDir());
}

TEST_F(ConfigTest, testParseDataDir)
{
    in.str(R"EOF(
data_dir = "/tmp/bridge"
)EOF"s);
    const auto config = Config {in};
    EXPECT_EQ("/tmp/bridge", config.getDataDir());
}

TEST_F(ConfigTest, testKnownNodes)
{
    in.str(R"EOF(
known_nodes = {
  { public_key = "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7", user_id = "node1" },
  { public_key = "JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6", user_id = "node2" },
}
)EOF"s);
    const auto config = Config {in};
    const auto expected_known_nodes = NodeMap {
        { decodeKey("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"), "node1" },
        { decodeKey("JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6"), "node2" },
    };
    EXPECT_EQ(expected_known_nodes, config.getKnownNodes());
}

TEST_F(ConfigTest, testKnownNodesWrongType)
{
    in.str(R"EOF(
known_nodes = "invalid"
)EOF"s);
    const auto config = Config {in};
    EXPECT_TRUE(config.getKnownNodes().empty());
}

TEST_F(ConfigTest, testKnownNodesMissingPublicKey)
{
    in.str(R"EOF(
known_nodes = {
  { user_id = "node1" },
}
)EOF"s);
    const auto config = Config {in};
    EXPECT_TRUE(config.getKnownNodes().empty());
}

TEST_F(ConfigTest, testKnownNodesInvalidPublicKey)
{
    in.str(R"EOF(
known_nodes = {
  { public_key = "invalid", user_id = "node1" },
}
)EOF"s);
    const auto config = Config {in};
    EXPECT_TRUE(config.getKnownNodes().empty());
}

TEST_F(ConfigTest, testParseGameConfig)
{
    in.str(R"EOF(
game { uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490" }
)EOF"s);
    const auto config = Config {in};
    const auto& games = config.getGameConfigs();
    ASSERT_EQ(1u, games.size());
    auto uuid_generator = boost::uuids::string_generator {};
    const auto& game = games.front();
    EXPECT_EQ(
        uuid_generator("575332b4-fa13-4d65-acf6-9f24b5e2e490"),
        game.uuid);
    EXPECT_FALSE(game.isDefault);
}

TEST_F(ConfigTest, testParseGameConfigDefaultGame)
{
    in.str(R"EOF(
game { uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490", default = true }
)EOF"s);
    const auto config = Config {in};
    const auto& games = config.getGameConfigs();
    ASSERT_EQ(1u, games.size());
    const auto& game = games.front();
    EXPECT_TRUE(games.front().isDefault);
}

TEST_F(ConfigTest, testParseGameConfigWrongArgumentType)
{
    in.str("game(1)"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigMissingUuid)
{
    in.str("game {}"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigInvalidUuid)
{
    in.str(R"EOF(
game { uuid = "not uuid"}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigPositionsControlled)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    positions_controlled = { "north", "south" },
}
)EOF"s);
    const auto config = Config {in};
    const auto& games = config.getGameConfigs();
    ASSERT_EQ(1u, games.size());
    const auto expected_positions = std::vector {
        Bridge::Positions::NORTH, Bridge::Positions::SOUTH };
    EXPECT_EQ(expected_positions, games.front().positionsControlled);
}

TEST_F(ConfigTest, testParseGameConfigPositionsControlledInvalidPositionType)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    positions_controlled = { 1, 2, 3 },
}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigPositionsControlledInvalidPositionEnum)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    positions_controlled = { "invalid" },
}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigPeers)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    peers = {
        {
            endpoint = "test-endpoint-1",
            server_key = "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7",
        },
        {
            endpoint = "test-endpoint-2",
        },
    },
}
)EOF"s);
    const auto config = Config {in};
    const auto& games = config.getGameConfigs();
    ASSERT_EQ(1u, games.size());
    const auto decoded_key =
        decodeKey("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"sv);
    const auto expected_peers = std::vector {
        BridgeGameConfig::PeerConfig {
            "test-endpoint-1"s,
            decoded_key
        },
        BridgeGameConfig::PeerConfig { "test-endpoint-2"s, {} },
    };
    EXPECT_EQ(expected_peers, games.front().peers);
    const auto& known_nodes = config.getKnownNodes();
    EXPECT_TRUE(known_nodes.find(decoded_key) != known_nodes.end());
}

TEST_F(ConfigTest, testParseGameConfigPeersInvalidPeer)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    peers = { 123 },
}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigPeersPeerEndpointMissing)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    peers = {
        { key_which_is_not_endpoint = "something" }.
    },
}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigPeersInvalidServerKey)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    peers = {
        { endpoint = "test-endpoint-1", server_key = "invalid" },
    },
}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigCardServer)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    card_server = {
        control_endpoint = "control-endpoint",
        peer_endpoint = "peer-endpoint",
    },
}
)EOF"s);
    const auto config = Config {in};
    const auto& games = config.getGameConfigs();
    ASSERT_EQ(1u, games.size());
    const auto expected_cs_config = BridgeGameConfig::CardServerConfig {
        "control-endpoint"s, "peer-endpoint"s, {}};
    EXPECT_EQ(expected_cs_config, games.front().cardServer);
}

TEST_F(ConfigTest, testParseGameConfigCardServerMissingControlEndpoint)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    card_server = {
        peer_endpoint = "peer-endpoint",
    },
}
)EOF"s);
    assertThrows();
}

TEST_F(ConfigTest, testParseGameConfigCardServerMissingPeerEndpoint)
{
    in.str(R"EOF(
game {
    uuid = "575332b4-fa13-4d65-acf6-9f24b5e2e490",
    card_server = {
        control_endpoint = "control-endpoint",
    },
}
)EOF"s);
    assertThrows();
}
