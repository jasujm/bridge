#include "main/Config.hh"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>

using namespace std::string_literals;

using Bridge::Main::Config;

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
    EXPECT_EQ("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"s, curve->serverKey);
    EXPECT_EQ("JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6"s, curve->secretKey);
    EXPECT_EQ("rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"s, curve->publicKey);
}
