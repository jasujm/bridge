#include "messaging/DuplicateResultJsonSerializer.hh"

#include "bridge/DuplicateScoring.hh"
#include "messaging/JsonSerializerUtility.hh"

using nlohmann::json;

namespace Bridge {

const std::string DUPLICATE_RESULT_PARTNERSHIP_KEY {"partnership"};
const std::string DUPLICATE_RESULT_SCORE_KEY {"score"};

void to_json(json& j, const DuplicateResult& result)
{
    j.emplace(DUPLICATE_RESULT_PARTNERSHIP_KEY, result.partnership);
    j.emplace(DUPLICATE_RESULT_SCORE_KEY, result.score);
}

void from_json(const json& j, DuplicateResult& result)
{
    result.partnership = j.at(DUPLICATE_RESULT_PARTNERSHIP_KEY)
        .get<std::optional<Partnership>>();
    result.score = j.at(DUPLICATE_RESULT_SCORE_KEY);
}

}
