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

#include "messaging/JsonSerializer.hh"

#include <string>

namespace Bridge {

enum class Strain;
class Bid;

namespace Messaging {

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

/** \brief JSON converter for Strain
 *
 * \sa JsonSerializer.hh, \ref jsonbid
 */
template<>
struct JsonConverter<Strain>
{
    /** \brief Convert Strain to JSON
     */
    static nlohmann::json convertToJson(Strain strain);

    /** \brief Convert JSON to Strain
     */
    static Strain convertFromJson(const nlohmann::json& j);
};

/** \brief JSON converter for Bid
 *
 * \sa JsonSerializer.hh, \ref jsonbid
 */
template<>
struct JsonConverter<Bid>
{
    /** \brief Convert Bid to JSON
     */
    static nlohmann::json convertToJson(const Bid& strain);

    /** \brief Convert JSON to Bid
     */
    static Bid convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_BIDJSONSERIALIZER_HH_
