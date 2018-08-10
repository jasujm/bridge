#include "main/Config.hh"
#include "messaging/EndpointIterator.hh"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>

using namespace std::string_literals;

using Bridge::Main::Config;
using Bridge::Messaging::decodeKey;

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
    EXPECT_EQ(expectedPublicKey, curve->serverKey);
    EXPECT_EQ(expectedSecretKey, curve->secretKey);
    EXPECT_EQ(expectedPublicKey, curve->publicKey);
}
