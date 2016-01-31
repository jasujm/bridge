#include "bridge/CardType.hh"
#include "bridge/Bid.hh"
#include "bridge/Call.hh"
#include "messaging/MessageHandlingException.hh"
#include "messaging/JsonSerializer.hh"
#include "messaging/BidJsonSerializer.hh"
#include "messaging/CallJsonSerializer.hh"
#include "messaging/CardTypeJsonSerializer.hh"

#include <json.hpp>
#include <gtest/gtest.h>

using namespace Bridge;
using namespace Bridge::Messaging;

using nlohmann::json;

class JsonSerializerTest : public testing::Test {
protected:

    template<typename T>
    void testHelper(const T& t, const json& j)
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
    void testFailedDeserializationHelper(const json& j)
    {
        EXPECT_THROW(
            serializer.deserialize<T>(j.dump()),
            MessageHandlingException);
    }

private:
    JsonSerializer serializer;
};

TEST_F(JsonSerializerTest, testGeneral)
{
    const auto message = std::string {"hello"};
    testHelper(message, message);
}

TEST_F(JsonSerializerTest, testBid)
{
    const auto j = json {
        {BID_LEVEL_KEY, json(4)},
        {BID_STRAIN_KEY, json(STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS))}};
    const auto bid = Bid {4, Strain::HEARTS};
    testHelper(bid, j);
}

TEST_F(JsonSerializerTest, testBidMissingLevel)
{
    const auto j = json {
        {BID_STRAIN_KEY, STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS)}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidLevelNotInteger)
{
    const auto j = json {
        {BID_LEVEL_KEY, json {}},
        {BID_STRAIN_KEY, json(STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS))}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidLevelInvalid)
{
    const auto j = json {
        {BID_LEVEL_KEY, Bid::MAXIMUM_LEVEL + 1},
        {BID_STRAIN_KEY, json(STRAIN_TO_STRING_MAP.left.at(Strain::HEARTS))}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidMissingStrain)
{
    const auto j = json {{BID_LEVEL_KEY, 4}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidStrainNotString)
{
    const auto j = json {{BID_LEVEL_KEY, json(4)}, {BID_STRAIN_KEY, json {}}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testBidStrainInvalid)
{
    const auto j = json {
        {BID_LEVEL_KEY, json(4)}, {BID_STRAIN_KEY, json("invalid")}};
    testFailedDeserializationHelper<Bid>(j);
}

TEST_F(JsonSerializerTest, testCallPass)
{
    const auto j = json {{CALL_TYPE_KEY, CALL_PASS_TAG}};
    const auto call = Call {Pass {}};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallBid)
{
    const auto bid = Bid {4, Strain::HEARTS};
    const auto j = json {
        {CALL_TYPE_KEY, json(CALL_BID_TAG)}, {CALL_BID_TAG, toJson(bid)}};
    const auto call = Call {bid};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallDouble)
{
    const auto j = json {{CALL_TYPE_KEY, CALL_DOUBLE_TAG}};
    const auto call = Call {Double {}};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallRedouble)
{
    const auto j = json {{CALL_TYPE_KEY, CALL_REDOUBLE_TAG}};
    const auto call = Call {Redouble {}};
    testHelper(call, j);
}

TEST_F(JsonSerializerTest, testCallCallMissing)
{
    const auto j = json::object();
    testFailedDeserializationHelper<Call>(j);
}

TEST_F(JsonSerializerTest, testCardType)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, RANK_TO_STRING_MAP.left.at(Rank::ACE)},
        {CARD_TYPE_SUIT_KEY, SUIT_TO_STRING_MAP.left.at(Suit::SPADES)}};
    const auto card_type = CardType {Rank::ACE, Suit::SPADES};
    testHelper(card_type, j);
}

TEST_F(JsonSerializerTest, testCardTypeRankMissing)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, RANK_TO_STRING_MAP.left.at(Rank::ACE)}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeRankInvalid)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, "invalid"},
        {CARD_TYPE_SUIT_KEY, SUIT_TO_STRING_MAP.left.at(Suit::SPADES)}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeSuitMissing)
{
    const auto j = json {
        {CARD_TYPE_SUIT_KEY, SUIT_TO_STRING_MAP.left.at(Suit::SPADES)}};
    testFailedDeserializationHelper<CardType>(j);
}

TEST_F(JsonSerializerTest, testCardTypeSuitInvalid)
{
    const auto j = json {
        {CARD_TYPE_RANK_KEY, RANK_TO_STRING_MAP.left.at(Rank::ACE)},
        {CARD_TYPE_SUIT_KEY, "invalid"}};
    testFailedDeserializationHelper<CardType>(j);
}
