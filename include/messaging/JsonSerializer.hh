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

#include <json.hpp>

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
 * \note JsonSerializer::deserialize() catches all exceptions from the JSON
 * library. While this somewhat contradicts the general design purpose of
 * limiting scopes of try clauses in order not to mask bugs, this is
 * purposefully done to greatly simplify implementing JsonConverter for
 * individual types.
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
    template<typename T> static std::string serialize(const T& t)
    {
        return nlohmann::json(t).dump();
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
    template<typename T> static T deserialize(const auto& s)
    {
        try {
            return nlohmann::json::parse(s).template get<T>();
        } catch (const nlohmann::json::exception&) {
            throw SerializationFailureException {};
        }
    }
};

}
}

#endif // MESSAGING_JSONSERIALIZER_HH_
