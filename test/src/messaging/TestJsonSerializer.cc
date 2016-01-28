#include "messaging/JsonSerializer.hh"

#include <json.hpp>
#include <gtest/gtest.h>

using Bridge::Messaging::JsonSerializer;

using nlohmann::json;

namespace {
const auto message = std::string {"hello"};
}

class JsonSerializerTest : public testing::Test {
protected:

    template<typename T>
    void testSerializationHelper(const T& t)
    {
        // Using EXPECT_EQ causes stack overflow is operations are
        // unsuccessful (because printing the error messages them would
        // require them to be successful)
        EXPECT_TRUE(j == json::parse(serializer.serialize(t)))
            << "Failed to serialize:\n" << t;
        EXPECT_TRUE(t == serializer.deserialize<T>(j.dump()))
            << "Failed to deserialize:\n" << j;
    }

    template<typename T>
    void testDeserializationHelper(const T& t)
    {
        EXPECT_EQ(t, serializer.deserialize<T>(json(t).dump()));
    }

private:
    JsonSerializer serializer;
};

TEST_F(JsonSerializerTest, testSerialization)
{
    testSerializationHelper(message);
}

TEST_F(JsonSerializerTest, testDeserialization)
{
    testDeserializationHelper(message);
}
