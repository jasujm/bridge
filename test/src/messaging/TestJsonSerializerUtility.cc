#include "messaging/JsonSerializerUtility.hh"

#include <gtest/gtest.h>
#include <json.hpp>

#include <string>

using Bridge::Blob;
using Bridge::Messaging::SerializationFailureException;

namespace {
using namespace std::string_literals;
const auto TEST_STRING = "test"s;
const auto TEST_BLOB = Blob { std::byte {123}, std::byte {32} };
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
    EXPECT_EQ(Bridge::Messaging::toJson(TEST_BLOB), TEST_HEX);
}

TEST(JsonSerializerUtilityTest, testJsonToBlob)
{
    EXPECT_EQ(Bridge::Messaging::fromJson<Blob>(TEST_HEX), TEST_BLOB);
}

TEST(JsonSerializerUtilityTest, testJsonToBlobInvalidType)
{
    EXPECT_THROW(Bridge::Messaging::fromJson<Blob>(123), SerializationFailureException);
}

TEST(JsonSerializerUtilityTest, testJsonToBlobInvalidHex)
{
    EXPECT_THROW(Bridge::Messaging::fromJson<Blob>("xx"s), SerializationFailureException);
}
