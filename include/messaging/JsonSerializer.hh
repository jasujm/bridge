/** \file
 *
 * \brief Definition of JSON serialization policy for message passing
 *
 * The serializer is based on the JSON library by nlohmann
 * (https://github.com/nlohmann/json).
 */

#ifndef MESSAGING_JSONSERIALIZER_HH_
#define MESSAGING_JSONSERIALIZER_HH_

#include "messaging/MessageHandlingException.hh"

#include <json.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Convert object to JSON
 *
 * \note To be able to serialize complex types with JsonSerializer, this
 * template can be specialized.
 *
 * \param t the object to convert
 *
 * \return JSON object implicitly constructed from \p t
 */
template<typename T>
nlohmann::json toJson(const T& t)
{
    return t;
}

/** \brief Convert JSON to object of type \p T
 *
 * \note To be able to deserialize complex types with JsonSerializer, this
 * template can be specialized.
 *
 * \param j the JSON object to convert
 *
 * \return \p T object implicitly converted from \p j
 *
 * \throw MessageHandlingException if \p j is not compatible with \p T
 * (see more in nlohmann::json documentation)
 */
template<typename T>
T fromJson(const nlohmann::json& j) {
    try {
        return j;
    } catch (const std::domain_error&) {
        // We rely on json::operator T() throwing std::domain_error if the
        // j and T are incompatible
        throw MessageHandlingException {};
    }
}

/** \brief Serialization policy that uses JSON
 *
 * This serializer can be used to exchange messages in JSON format. It uses
 * toJson() and fromJson() or their specializations to handle the actual
 * conversions to and from JSON, respectively.
 *
 * \sa FunctionMessageHandler
 */
struct JsonSerializer {

    /** \brief Serialize object to string
     *
     * \param t the object to serialize
     *
     * \return String representation of the JSON \p t is convered to using
     * toJson() or its specialization
     */
    template<typename T> static std::string serialize(const T& t)
    {
        return toJson(t).dump();
    }

    /** \brief Deserialize string to object
     *
     * \param s the string to deserialize
     *
     * \return object retrieved by parsing \p s and then using fromJson() or
     * its specialization
     */
    template<typename T> static T deserialize(const std::string& s)
    {
        return fromJson<T>(nlohmann::json::parse(s));
    }
};

}
}

#endif // MESSAGING_JSONSERIALIZER_HH_
