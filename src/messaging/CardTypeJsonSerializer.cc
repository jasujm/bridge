#include "messaging/CardTypeJsonSerializer.hh"

#include "bridge/CardType.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

const std::string CARD_TYPE_RANK_KEY {"rank"};
const std::string CARD_TYPE_SUIT_KEY {"suit"};

void to_json(json& j, const Rank rank)
{
    j = Messaging::enumToJson(rank, RANK_TO_STRING_MAP.left);
}

void from_json(const json& j, Rank& rank)
{
    rank = Messaging::jsonToEnum<Rank>(j, RANK_TO_STRING_MAP.right);
}

void to_json(json& j, const Suit suit)
{
    j = Messaging::enumToJson(suit, SUIT_TO_STRING_MAP.left);
}

void from_json(const json& j, Suit& suit)
{
    suit = Messaging::jsonToEnum<Suit>(j, SUIT_TO_STRING_MAP.right);
}

void to_json(json& j, const CardType& cardType)
{
    j[CARD_TYPE_RANK_KEY] = cardType.rank;
    j[CARD_TYPE_SUIT_KEY] = cardType.suit;
}

void from_json(const json& j, CardType& cardType)
{
    cardType.rank = j.at(CARD_TYPE_RANK_KEY);
    cardType.suit = j.at(CARD_TYPE_SUIT_KEY);
}

}
