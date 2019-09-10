#include "HexUtility.hh"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

using Bridge::encodeHex;
using Bridge::decodeHex;
using Bridge::toHex;
using Bridge::fromHex;
using Bridge::isValidHex;

using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;

namespace {

const auto BYTES = "\x01\x23\x45\x67\x89\xab\xcd\xef"_BS;
const auto HEX = "0123456789abcdef"sv;

}

TEST(HexUtilityTest, testEncode)
{
    auto output = std::string(16, '\0');
    EXPECT_EQ(
        output.end(), encodeHex(BYTES.begin(), BYTES.end(), output.begin()));
    EXPECT_EQ(output, HEX);
}

TEST(HexUtilityTest, testDecode)
{
    auto output = std::vector<std::byte>(8);
    EXPECT_EQ(output.end(), decodeHex(HEX.begin(), HEX.end(), output.begin()));
    EXPECT_EQ(output, BYTES);
}

TEST(HexUtilityTest, testInvalidLength)
{
    const auto input = "012"sv;
    auto output = std::vector<std::byte>(input.size());
    EXPECT_THROW(
        decodeHex(input.begin(), input.end(), output.begin()),
        std::runtime_error);
}

TEST(HexUtilityTest, testInvalidCharacters)
{
    const auto input = "xx"sv;
    auto output = std::vector<std::byte>(input.size());
    EXPECT_THROW(
        decodeHex(input.begin(), input.end(), output.begin()),
        std::runtime_error);
}

TEST(HexUtilityTest, testValidHex)
{
    EXPECT_TRUE(isValidHex(HEX.begin(), HEX.end()));
}

TEST(HexUtilityTest, testInvalidHexOddLength)
{
    EXPECT_FALSE(isValidHex(HEX.begin(), HEX.end() - 1));
}

TEST(HexUtilityTest, testInvalidHexInvalidChars)
{
    const auto input = "xx"sv;
    EXPECT_FALSE(isValidHex(input.begin(), input.end()));
}

TEST(HexUtilityTest, testToHex) {
    EXPECT_EQ(HEX, toHex(BYTES));
}

TEST(HexUtilityTest, testFromHex) {
    EXPECT_EQ(BYTES, fromHex(HEX));
}

TEST(HexUtilityTest, testFormatHex)
{
    std::ostringstream s;
    s << Bridge::formatHex(BYTES);
    EXPECT_EQ(HEX, s.str());
}
