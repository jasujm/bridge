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

using namespace std::string_literals;

namespace {

const auto BYTES = std::vector {
    std::byte {1}, std::byte {35}, std::byte {69}, std::byte {103},
    std::byte {137}, std::byte {171}, std::byte {205}, std::byte {239}};
const auto HEX = "0123456789abcdef"s;

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
    const auto input = "012"s;
    auto output = std::vector<std::byte>(input.size());
    EXPECT_THROW(
        decodeHex(input.begin(), input.end(), output.begin()),
        std::runtime_error);
}

TEST(HexUtilityTest, testInvalidCharacters)
{
    const auto input = "xx"s;
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
    const auto input = "xx"s;
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
