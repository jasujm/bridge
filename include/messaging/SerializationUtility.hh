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
 */

#ifndef MESSAGING_SERIALIZATIONUTILITY_HH_
#define MESSAGING_SERIALIZATIONUTILITY_HH_

#include <algorithm>
#include <tuple>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Deserialize several strings
 *
 * \param serializer see \ref serializationpolicy
 * \param strings the strings to deserialize
 *
 * \return std::tuple containing all deserialized values
 */
template<
    typename... DeserializedTypes, typename SerializationPolicy,
    typename... Strings>
auto deserializeAll(SerializationPolicy&& serializer, Strings&&... strings)
{
    static_assert(
        sizeof...(DeserializedTypes) == sizeof...(Strings),
        "Number of deserialized types and strings must match");
    return std::make_tuple(
        serializer.template deserialize<DeserializedTypes>(
            std::forward<Strings>(strings))...);
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
