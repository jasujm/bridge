/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Bid
 *
 * \page jsonbid Bid JSON representation
 *
 * A Bridge::Bid is represented by a JSON object consisting of the following:
 *
 * \code{.json}
 * {
 *     { "level": <level> },
 *     { "strain": <strain> }
 * }
 * \endcode
 *
 * - &lt;level&gt; is an integer between 1…7 representing the level of the bid
 * - &lt;strain&gt; is a string representing the strain of the bid. It must be
 *   one of the following: "clubs", "diamonds", "hearts", "spades", "notrump".
 */

#ifndef MESSAGING_BIDJSONSERIALIZER_HH_
#define MESSAGING_BIDJSONSERIALIZER_HH_

#include <nlohmann/json.hpp>

#include <string>

namespace Bridge {

struct Strain;
class Bid;

/** \brief Key for Bid::level in JSON object
 *
 * \sa \ref jsonbid
 */
extern const std::string BID_LEVEL_KEY;

/** \brief Key for Bid::strain in JSON object
 *
 * \sa \ref jsonbid
 */
extern const std::string BID_STRAIN_KEY;

/** \brief Convert Bid to JSON
 */
void to_json(nlohmann::json&, const Bid&);

/** \brief Convert JSON to Bid
 */
void from_json(const nlohmann::json&, Bid&);

}

#endif // MESSAGING_BIDJSONSERIALIZER_HH_
