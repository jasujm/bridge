/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Position
 */

#ifndef MESSAGING_POSITIONJSONSERIALIZER_HH_
#define MESSAGING_POSITIONJSONSERIALIZER_HH_

#include "messaging/JsonSerializer.hh"

namespace Bridge {

enum class Position;

namespace Messaging {

/** \brief JSON converter for Position
 *
 * \sa JsonSerializer.hh
 */
template<>
struct JsonConverter<Position>
{
    /** \brief Convert Position to JSON
     */
    static nlohmann::json convertToJson(Position position);

    /** \brief Convert JSON to Position
     */
    static Position convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_POSITIONJSONSERIALIZER_HH_
