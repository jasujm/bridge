#include "messaging/JsonSerializerUtility.hh"

#include <gtest/gtest.h>
#include <json.hpp>

#include <string>

using Bridge::Messaging::SerializationFailureException;

namespace {
using namespace std::string_literals;
using namespace Bridge::BlobLiterals;
const auto TEST_STRING = "test"s;
const auto TEST_BLOB = "\x7b\x20"_B;
const auto TEST_HEX = "7b20"s;
}

TEST(JsonSerializerUtilityTest, testTryFromJsonSuccess)
{
    const auto j = nlohmann::json(TEST_STRING);
    const auto s = Bridge::Messaging::tryFromJson<std::string>(j);
    EXPECT_EQ(TEST_STRING, s);
}

TEST(JsonSerializerUtilityTest, testTryFromJsonFailure)
{
    const auto j = nlohmann::json(TEST_STRING);
    const auto i = Bridge::Messaging::tryFromJson<int>(j);
    EXPECT_FALSE(i);
}

TEST(JsonSerializerUtilityTest, testBlobToJson)
{
    EXPECT_EQ(Bridge::Messaging::blobToJson(TEST_BLOB), TEST_HEX);
}

TEST(JsonSerializerUtilityTest, testJsonToBlob)
{
    EXPECT_EQ(Bridge::Messaging::jsonToBlob(TEST_HEX), TEST_BLOB);
}

TEST(JsonSerializerUtilityTest, testJsonToBlobInvalidType)
{
    EXPECT_THROW(Bridge::Messaging::jsonToBlob(123), SerializationFailureException);
}

TEST(JsonSerializerUtilityTest, testJsonToBlobInvalidHex)
{
    EXPECT_THROW(Bridge::Messaging::jsonToBlob("xx"s), SerializationFailureException);
}
