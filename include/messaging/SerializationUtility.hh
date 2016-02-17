/** \file
 *
 * \brief Definition of serialization utilities
 *
 * \page serializationpolicy SerializationPolicy concept
 *
 * This project uses serialization policy objects to translate to and from
 * string presentation of objects. Given object \c serializer and serializable
 * object \c t of type \c T, the following expressions must be valid and have
 * the following return values:
 *
 * Expression                                         | Return value
 * ---------------------------------------------------|--------------
 * serializer.serialize(t)                            | string
 * serializer.deserialize<T>(serializer.serialize(t)) | equal to \c t
 *
 * \c deserialize may in addition signal deserialization error by throwing an
 * instance of Messaging::MessageHandlingException. This allows
 * deserialization to throw an exception as soon as it detects an error at any
 * depth of call stack.
 */

#ifndef MESSAGING_SERIALIZATIONUTILITY_HH_
#define MESSAGING_SERIALIZATIONUTILITY_HH_

#include "messaging/MessageHandlingException.hh"

#include <boost/optional/optional.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Deserialize several strings
 *
 * This function is an utility used to deserialize arbitrary number of strings
 * at once and returning a tuple containing deserialized values. This function
 * also handles deserialization failures by returning no tuple in case any
 * deserialization fails. The order of deserializations is unspecified.
 *
 * \tparam DeserializedTypes the types the strings will be deserialized into
 *
 * \param serializer see \ref serializationpolicy
 * \param strings the strings to deserialize
 *
 * \return optional std::tuple containing all deserialized values. If
 * deserialization of any parameter fails (any call to deserialize throws
 * MessageHandlingException), none is returned.
 */
template<
    typename... DeserializedTypes, typename SerializationPolicy,
    typename... Strings>
boost::optional<std::tuple<DeserializedTypes...>> deserializeAll(
    SerializationPolicy&& serializer, Strings&&... strings)
{
    static_assert(
        sizeof...(DeserializedTypes) == sizeof...(Strings),
        "Number of deserialized types and strings must match");
    try {
        return std::make_tuple(
            serializer.template deserialize<DeserializedTypes>(
                std::forward<Strings>(strings))...);
    } catch (const MessageHandlingException&) {
        return boost::none;
    }
}

/// \{

/** \brief Serialize several objects
 *
 * \param out output iterator the results of serialization are written to
 * \param serializer see \ref serializationpolicy
 * \param args the objects to serialize
 */
template<
    typename SerializationPolicy, typename OutputIterator, typename... Args>
void serializeAll(
    OutputIterator out, SerializationPolicy&& serializer, Args&&... args)
{
    auto&& serialized_values = {
        serializer.serialize(std::forward<Args>(args))...};
    std::move(serialized_values.begin(), serialized_values.end(), out);
}

template<typename SerializationPolicy, typename OutputIterator>
void serializeAll(OutputIterator, SerializationPolicy&&)
{
}

/// \}

}
}

#endif // MESSAGING_SERIALIZATIONUTILITY_HH_
