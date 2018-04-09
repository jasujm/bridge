#include "messaging/DuplicateScoreJsonSerializer.hh"

#include "scoring/DuplicateScore.hh"
#include "messaging/JsonSerializerUtility.hh"
#include "messaging/PartnershipJsonSerializer.hh"

using nlohmann::json;

namespace Bridge {

using Scoring::DuplicateScore;

namespace Messaging {

const std::string DUPLICATE_SCORE_PARTNERSHIP_KEY {"partnership"};
const std::string DUPLICATE_SCORE_SCORE_KEY {"score"};

json JsonConverter<DuplicateScore>::convertToJson(const DuplicateScore& score)
{
    return {
        {DUPLICATE_SCORE_PARTNERSHIP_KEY, toJson(score.partnership)},
        {DUPLICATE_SCORE_SCORE_KEY, toJson(score.score)}};
}

DuplicateScore JsonConverter<DuplicateScore>::convertFromJson(const json& j)
{
    return {
        checkedGet<Partnership>(j, DUPLICATE_SCORE_PARTNERSHIP_KEY),
        checkedGet<int>(j, DUPLICATE_SCORE_SCORE_KEY)};
}

}
}
