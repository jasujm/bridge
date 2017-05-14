/** \file
 *
 * \brief Definition of JSON serializer for UUID
 */

#ifndef MESSAGING_UUIDJSONSERIALIZER_HH_
#define MESSAGING_UUIDJSONSERIALIZER_HH_

#include "bridge/Player.hh" // For importing definition of Uuid
#include "messaging/JsonSerializer.hh"

namespace Bridge {
namespace Messaging {

/** \brief JSON converter for UUID
 *
 * \sa JsonSerializer.hh
 */
template<>
struct JsonConverter<Uuid>
{
    /** \brief Convert UUID to JSON
     */
    static nlohmann::json convertToJson(const Uuid& uuid);

    /** \brief Convert JSON to UUID
     */
    static Uuid convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_UUIDJSONSERIALIZER_HH_
