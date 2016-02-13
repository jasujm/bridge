/** \file
 *
 * \brief Definition of JSON serializer for Bridge::TricksWon
 *
 * \page jsontrickswon TricksWon JSON representation
 *
 * A Bridge::TricksWon is represented by a JSON object consisting of the
 * following:
 *
 * \code{.json}
 * {
 *     { "northSouth": <northSouthTricksWon> },
 *     { "eastWest": <eastWestTricksWon> }
 * }
 * \endcode
 *
 * - &lt;northSouthTricksWon&gt; is an integer indicating the number of
 *   tricks won by the north–south partnership
 * - &lt;eastWestTricksWon&gt; is an integer indicating the number of
 *   tricks won by the east–west partnership
 */

#ifndef MESSAGING_TRICKSWONJSONSERIALIZER_HH_
#define MESSAGING_TRICKSWONJSONSERIALIZER_HH_

#include "messaging/JsonSerializer.hh"

namespace Bridge {

class TricksWon;

namespace Messaging {

/** \brief JSON converter for TricksWon
 *
 * \sa JsonSerializer.hh, \ref jsontrickswon
 */
template<>
struct JsonConverter<TricksWon>
{
    /** \brief Convert TricksWon to JSON
     */
    static nlohmann::json convertToJson(const TricksWon& tricksWon);

    /** \brief Convert JSON to TricksWon
     */
    static TricksWon convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_TRICKSWONJSONSERIALIZER_HH_
