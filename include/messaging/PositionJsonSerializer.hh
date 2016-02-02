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

/** \sa toJson() */
template<>
nlohmann::json toJson(const Position& position);

/** \sa fromJson() */
template<>
Position fromJson(const nlohmann::json& j);

}
}

#endif // MESSAGING_POSITIONJSONSERIALIZER_HH_
