/** \file
 *
 * \brief Definition of Bridge::Messaging::SerializationFailureException class
 */

#ifndef MESSAGING_SERIALIZATIONFAILUREEXCEPTION_HH_
#define MESSAGING_SERIALIZATIONFAILUREEXCEPTION_HH_

#include <exception>

namespace Bridge {
namespace Messaging {

/** \brief Exception to indicate error in serialization or deserialization
 *
 * This non-fatal exception is used by serialization policy to signal that the
 * serializer was unable to perform the serialization or deserialization.
 *
 * \sa \ref serializationpolicy
 */
class SerializationFailureException : public std::exception {};

}
}

#endif // MESSAGING_SERIALIZATIONFAILUREEXCEPTION_HH_
