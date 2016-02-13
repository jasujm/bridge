#include "messaging/CardTypeJsonSerializer.hh"

#include "bridge/CardType.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string CARD_TYPE_RANK_KEY {"rank"};
const std::string CARD_TYPE_SUIT_KEY {"suit"};

json JsonConverter<Rank>::convertToJson(const Rank rank)
{
    return enumToJson(rank, RANK_TO_STRING_MAP.left);
}

Rank JsonConverter<Rank>::convertFromJson(const json& j)
{
    return jsonToEnum<Rank>(j, RANK_TO_STRING_MAP.right);
}

json JsonConverter<Suit>::convertToJson(const Suit suit)
{
    return enumToJson(suit, SUIT_TO_STRING_MAP.left);
}

Suit JsonConverter<Suit>::convertFromJson(const json& j)
{
    return jsonToEnum<Suit>(j, SUIT_TO_STRING_MAP.right);
}

json JsonConverter<CardType>::convertToJson(const CardType& cardType)
{
    return {
        {CARD_TYPE_RANK_KEY, toJson(cardType.rank)},
        {CARD_TYPE_SUIT_KEY, toJson(cardType.suit)}};
}

CardType JsonConverter<CardType>::convertFromJson(const json& j)
{
    return {
        checkedGet<Rank>(j, CARD_TYPE_RANK_KEY),
        checkedGet<Suit>(j, CARD_TYPE_SUIT_KEY)};
}

}
}
