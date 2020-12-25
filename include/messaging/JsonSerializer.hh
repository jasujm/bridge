/** \file
 *
 * \brief Definition of JSON serialization policy for message passing
 *
 * The file defines serialization policy based on the JSON library by nlohmann
 * (https://github.com/nlohmann/json). It can be used in conjuction with other
 * messaging utilities to pass objects as messages.
 *
 * \sa \ref serializationpolicy
 */

#ifndef MESSAGING_JSONSERIALIZER_HH_
#define MESSAGING_JSONSERIALIZER_HH_

#include "messaging/SerializationFailureException.hh"
#include "Blob.hh"

#include <nlohmann/json.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Serialization policy that uses JSON
 *
 * This serializer can be used to exchange messages in JSON format. It uses the
 * nlohmann::json library to serialize objects into JSON and dumps them as
 * string. For deserialization it parses the incoming string as JSON and
 * converts to the desired type.
 *
 * \sa FunctionMessageHandler
 */
struct JsonSerializer {

    /** \brief Serialize object to string
     *
     * \param t the object to serialize
     *
     * \return string dump of the JSON object resulting from converting \p t
     */
    template<typename T> static std::string serialize(T&& t)
    {
        return nlohmann::json(std::forward<T>(t)).dump();
    }

    /** \brief Deserialize string to object
     *
     * \param s the string to deserialize
     *
     * \return object retrieved by parsing \p s into JSON and then deserializing
     * it to an object of type \c T
     *
     * \throw SerializationFailureException in case any exception is caught from
     * the JSON library
     */
    template<typename T, typename String>
    static T deserialize(String&& s)
    {
        try {
            // nlohmann::json 3.9.1 won't parse bytes directly, force to string
            static_assert(sizeof(*std::data(s)) == 1);
            const auto sv = std::string_view(
                reinterpret_cast<const char*>(std::data(s)), std::size(s));
            return nlohmann::json::parse(sv).template get<T>();
        } catch (const nlohmann::json::exception&) {
            throw SerializationFailureException {};
        }
    }
};

}
}

#endif // MESSAGING_JSONSERIALIZER_HH_
