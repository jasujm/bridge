/** \file
 *
 * \brief Definition of JSON serializer for Bridge::Partnership
 */

#ifndef MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_
#define MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_

#include "messaging/JsonSerializer.hh"

namespace Bridge {

enum class Partnership;

namespace Messaging {

/** \brief JSON converter for Partnership
 *
 * \sa JsonSerializer.hh
 */
template<>
struct JsonConverter<Partnership>
{
    /** \brief Convert Partnership to JSON
     */
    static nlohmann::json convertToJson(Partnership partnership);

    /** \brief Convert JSON to Partnership
     */
    static Partnership convertFromJson(const nlohmann::json& j);
};

}
}

#endif // MESSAGING_PARTNERSHIPJSONSERIALIZER_HH_
