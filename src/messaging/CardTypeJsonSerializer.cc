#include "messaging/CardTypeJsonSerializer.hh"

#include "bridge/CardType.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

const std::string CARD_TYPE_RANK_KEY {"rank"};
const std::string CARD_TYPE_SUIT_KEY {"suit"};

void to_json(json& j, const CardType& cardType)
{
    j.emplace(CARD_TYPE_RANK_KEY, cardType.rank);
    j.emplace(CARD_TYPE_SUIT_KEY, cardType.suit);
}

void from_json(const json& j, CardType& cardType)
{
    cardType.rank = j.at(CARD_TYPE_RANK_KEY);
    cardType.suit = j.at(CARD_TYPE_SUIT_KEY);
}

}
