/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageHandlingException class
 */

#ifndef MESSAGING_MESSAGEHANDLINGEXCEPTION_HH_
#define MESSAGING_MESSAGEHANDLINGEXCEPTION_HH_

#include <exception>

namespace Bridge {
namespace Messaging {

/** \brief Exception to indicate error in message handling
 *
 * This non-fatal exception is used by MessageHandler to signal that the
 * handler was unable to fulfill the request. The reason may be related to
 * malformed parameters (wrong number, wrong format etc.) or any other reason
 * (forbidden action etc.).
 */
class MessageHandlingException : public std::exception {};

}
}

#endif // MESSAGING_MESSAGEHANDLINGEXCEPTION_HH_
