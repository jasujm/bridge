#include "messaging/CardTypeJsonSerializer.hh"

#include "bridge/CardType.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {
namespace Messaging {

const std::string CARD_TYPE_RANK_KEY {"rank"};
const std::string CARD_TYPE_SUIT_KEY {"suit"};

template<>
json toJson(const Rank& rank)
{
    return enumToJson(rank, RANK_TO_STRING_MAP.left);
}

template<>
Rank fromJson(const json& j)
{
    return jsonToEnum<Rank>(j, RANK_TO_STRING_MAP.right);
}

template<>
json toJson(const Suit& suit)
{
    return enumToJson(suit, SUIT_TO_STRING_MAP.left);
}

template<>
Suit fromJson(const json& j)
{
    return jsonToEnum<Suit>(j, SUIT_TO_STRING_MAP.right);
}

template<>
json toJson(const CardType& cardType)
{
    return {
        {CARD_TYPE_RANK_KEY, toJson(cardType.rank)},
        {CARD_TYPE_SUIT_KEY, toJson(cardType.suit)}};
}

template<>
CardType fromJson(const json& j)
{
    return {
        checkedGet<Rank>(j, CARD_TYPE_RANK_KEY),
        checkedGet<Suit>(j, CARD_TYPE_SUIT_KEY)};
}

}
}
