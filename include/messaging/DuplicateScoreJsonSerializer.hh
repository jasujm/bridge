/** \file
 *
 * \brief Definition of JSON serializer for Scoring::DuplicateScore
 *
 * \page jsonduplicatescore Duplicate score JSON representation
 *
 * A Scoring::DuplicateScore is represented by a JSON object consisting of the
 * following:
 *
 * \code{.json}
 * {
 *     { "partnership": <partnership> },
 *     { "score": <score> }
 * }
 * \endcode
 *
 * - &lt;partnership&gt; is the partnership the score is awarded to, either
 *   "northSouth" or "eastWest"
 * - &lt;score&gt; is the amount scored (integer)
 */

#ifndef MESSAGING_DUPLICATESCOREJSONSERIALIZER_HH_
#define MESSAGING_DUPLICATESCOREJSONSERIALIZER_HH_

#include <json.hpp>

#include <string>

namespace Bridge {
namespace Scoring {

class DuplicateScore;

/** \brief Key for Scoring::DuplicateScore::partnership
 *
 * \sa \ref jsonduplicatescore
 */
extern const std::string DUPLICATE_SCORE_PARTNERSHIP_KEY;

/** \brief Key for Scoring::DuplicateScore::score
 *
 * \sa \ref jsonduplicatescore
 */
extern const std::string DUPLICATE_SCORE_SCORE_KEY;

/** \brief Convert DuplicateScore to JSON
 */
void to_json(nlohmann::json&, const DuplicateScore&);

/** \brief Convert JSON to DuplicateScore
 */
void from_json(const nlohmann::json& j, DuplicateScore&);

}
}

#endif // MESSAGING_DUPLICATESCOREJSONSERIALIZER_HH_
