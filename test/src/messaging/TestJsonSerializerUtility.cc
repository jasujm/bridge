#include "messaging/JsonSerializerUtility.hh"

#include <gtest/gtest.h>
#include <json.hpp>

#include <string>

namespace {
using namespace std::string_literals;
const auto TEST_STRING = "test"s;
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
