#include "messaging/DuplicateScoreJsonSerializer.hh"

#include "messaging/JsonSerializerUtility.hh"
#include "scoring/DuplicateScore.hh"

using nlohmann::json;

namespace Bridge {
namespace Scoring {

const std::string DUPLICATE_SCORE_PARTNERSHIP_KEY {"partnership"};
const std::string DUPLICATE_SCORE_SCORE_KEY {"score"};

void to_json(json& j, const DuplicateScore& score)
{
    j[DUPLICATE_SCORE_PARTNERSHIP_KEY] = score.partnership;
    j[DUPLICATE_SCORE_SCORE_KEY] = score.score;
}

void from_json(const json& j, DuplicateScore& score)
{
    score.partnership = j.at(DUPLICATE_SCORE_PARTNERSHIP_KEY);
    score.score = j.at(DUPLICATE_SCORE_SCORE_KEY);
}

}
}
