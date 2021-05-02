/** \file
 *
 * \brief Definition of JSON serializer for Bridge::DuplicateResult
 *
 * \page jsonduplicateresult Duplicate deal result JSON representation
 *
 * A Bridge::DuplicateResult is represented by a JSON object consisting of the
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
 *   "northSouth" or "eastWest", or null if the deal passed out
 * - &lt;score&gt; is the amount scored, or 0 if the deal passed out
 */

#ifndef MESSAGING_DUPLICATERESULTJSONSERIALIZER_HH_
#define MESSAGING_DUPLICATERESULTJSONSERIALIZER_HH_

#include <nlohmann/json.hpp>

#include <string>

namespace Bridge {

struct DuplicateResult;

/** \brief Key for Bridge::DuplicateResult::partnership
 *
 * \sa \ref jsonduplicateresult
 */
extern const std::string DUPLICATE_RESULT_PARTNERSHIP_KEY;

/** \brief Key for Bridge::DuplicateResult::result
 *
 * \sa \ref jsonduplicateresult
 */
extern const std::string DUPLICATE_RESULT_SCORE_KEY;

/** \brief Convert DuplicateResult to JSON
 */
void to_json(nlohmann::json&, const DuplicateResult&);

/** \brief Convert JSON to DuplicateResult
 */
void from_json(const nlohmann::json& j, DuplicateResult&);

}

#endif // MESSAGING_DUPLICATERESULTJSONSERIALIZER_HH_
