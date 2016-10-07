/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Call
 *
 * \page jsoncall Call JSON representation
 *
 * A Bridge::Call is represented by a JSON object consisting at least of the
 * following:
 *
 * \code{.json}
 * {
 *     { "type": <call> }
 * }
 * \endcode
 *
 * - &lt;call&gt; is a string representing the type of the call. It must be
 *   one of the following: "pass", "bid", "double", "redouble".
 *
 * If &lt;call&gt; is "bid", then the object in addition includes the bid:
 *
 * \code{.json}
 * {
 *     { "type": "bid" },
 *     { "bid": <bid> }
 * }
 * \endcode
 *
 * - &lt;bid&gt; is a JSON object describing the bid, see \ref jsonbid
 */

#ifndef MESSAGING_CALLJSONSERIALIZER_HH_
#define MESSAGING_CALLJSONSERIALIZER_HH_

#include "bridge/Call.hh"
#include "messaging/JsonSerializer.hh"

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Key for Call type
 *
 * \sa \ref jsoncall
 */
extern const std::string CALL_TYPE_KEY;

/** \brief Tag for Call type Pass
 *
 * \sa \ref jsoncall
 */
extern const std::string CALL_PASS_TAG;

/** \brief Tag for Call type Bid
 *
 * \sa \ref jsoncall
 */
extern const std::string CALL_BID_TAG;

/** \brief Tag for Call type Double
 *
 * \sa \ref jsoncall
 */
extern const std::string CALL_DOUBLE_TAG;

/** \brief Tag for Call type Redouble
 *
 * \sa \ref jsoncall
 */
extern const std::string CALL_REDOUBLE_TAG;

/** \brief JSON converter for Call
 *
 * \sa JsonSerializer.hh, \ref jsoncall
 */
template<>
struct JsonConverter<Call>
{
    /** \brief Convert Call to JSON
     */
    static nlohmann::json convertToJson(const Call& call);

    /** \brief Convert JSON to Call
     */
    static Call convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_CALLJSONSERIALIZER_HH_
