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

#include <json.hpp>

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Convert object to/from JSON
 *
 * JsonConverter is a struct that can be specialized to allow the JSON
 * serialization framework to convert different types to/from JSON. The
 * general template relies on implicit conversions provided by the JSON
 * library itself. Partial or full specializations of the struct can be used
 * to convert complex types for which impicit conversion does not work.
 *
 * \note Specializations of JsonConverter class can assume that any exceptions
 * from nlohmann::json library are caught by JsonSerializer. This means that
 * convertFromJson() can optimistically cast JSON objects to the desired types,
 * access desired keys in objects etc. without needing to do additional type
 * checking.
 *
 * \tparam T the type converted to/from JSON by the specialization
 */
template<typename T>
struct JsonConverter
{

    /** \brief Convert object to JSON
     *
     * The implementation in the general template just tries to convert object
     * implicitly to nlohmann::json object. If implicit conversion does not
     * exist, the program is ill‐formed.
     */
    static nlohmann::json convertToJson(const T& t);

    /** \brief Convert JSON to object
     *
     * The implementation in the general template just tries to convert object
     * implicitly from nlohmann::json object. If implicit conversion does not
     * exist, the program is ill‐formed.
     *
     * \throw SerializationFailureException if the conversion is unsuccessful
     */
    static T convertFromJson(const nlohmann::json& j);
};

template<typename T>
nlohmann::json JsonConverter<T>::convertToJson(const T& t)
{
    return t;
}

template<typename T>
T JsonConverter<T>::convertFromJson(const nlohmann::json& j)
{
    return j;
}

/** \brief Convert object to JSON
 *
 * This is convenience function for invoking JsonConverter<T>::convertToJson().
 *
 * \param t the object to convert
 *
 * \return JSON representation of \p t
 */
template<typename T>
nlohmann::json toJson(const T& t)
{
    return JsonConverter<T>::convertToJson(t);
}

/** \brief Convert JSON to object
 *
 * This is convenience function for invoking
 * JsonConverter<T>::convertFromJson().
 *
 * \param j the JSON object to convert
 *
 * \return \p j converted to object of type \p T
 */
template<typename T>
T fromJson(const nlohmann::json& j) {
    return JsonConverter<T>::convertFromJson(j);
}

/** \brief Serialization policy that uses JSON
 *
 * This serializer can be used to exchange messages in JSON format. It uses
 * toJson() and fromJson() or their specializations to handle the actual
 * conversions to and from JSON, respectively.
 *
 * \note JsonSerializer::deserialize() catches all exceptions from the JSON
 * library. While this somewhat contradicts the general design purpose of
 * limitnig scopes of try clauses in order not to mask bugs, this is
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
        try {
            return fromJson<T>(nlohmann::json::parse(s));
        } catch (const nlohmann::json::exception&) {
            throw SerializationFailureException {};
        }
    }
};

}
}

#endif // MESSAGING_JSONSERIALIZER_HH_
