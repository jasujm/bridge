/** \file
 *
 * \brief Definition of serialization utilities
 *
 * \page serializationpolicy SerializationPolicy concept
 *
 * This project uses serialization policy objects to translate to and from the
 * “wire” presentation of objects. Given object \c serializer and serializable
 * object \c t of type \c T, the following expressions must be valid and have
 * the following return values:
 *
 * Expression                                         | Return value
 * ---------------------------------------------------|--------------
 * serializer.serialize(t)                            | see below
 * serializer.deserialize<T>(serializer.serialize(t)) | equal to \c t
 *
 * The serialized object must be a contiguous sequence of 1‐byte objects. Given
 * such object \c c, \c std::data(c) must return pointer to the beginning of the
 * sequence and \c std::size(c) must return the size of the sequence.
 *
 * \c deserialize may in addition signal deserialization error by throwing an
 * instance of Bridge::Messaging::SerializationFailureException. This allows
 * deserialization to throw an exception as soon as it detects an error at any
 * depth of the call stack.
 */

#ifndef MESSAGING_SERIALIZATIONUTILITY_HH_
#define MESSAGING_SERIALIZATIONUTILITY_HH_

#include "messaging/SerializationFailureException.hh"
#include "Blob.hh"
#include "Utility.hh"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Find and deserialize single parameter from a range
 *
 * Given a range <tt>[first…last]</tt> of frames containing key–value pairs,
 * this function finds the value corresponding to the given key, deserializes it
 * and returns the deserialized value (or not).
 *
 * \tparam T type of the parameter to deserialize
 *
 * \param serializer see \ref serializationpolicy
 * \param first iterator to the first frame
 * \param last iterator one past the last frame
 * \param param the parameter to find
 *
 * \return the deserialized parameter corresponding to \p param, or nullopt if
 * the parameter is not found in the range
 */
template<typename T, typename SerializationPolicy, typename ParamIterator>
std::optional<T> deserializeParam(
    SerializationPolicy&& serializer,
    ParamIterator first, ParamIterator last, const auto& param)
{
    first = std::find_if(
        first, last,
        [param = asBytes(param)](const auto& part)
        {
            return asBytes(part) == asBytes(param);
        });
    if (first != last) {
        ++first;
        if (first != last) {
            return std::forward<SerializationPolicy>(serializer)
                .template deserialize<T>(asBytes(*first));
        }
    }
    return std::nullopt;
}

}
}

#endif // MESSAGING_SERIALIZATIONUTILITY_HH_
