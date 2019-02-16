/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageHandler interface
 */

#ifndef MESSAGING_MESSAGEHANDLER_HH_
#define MESSAGING_MESSAGEHANDLER_HH_

#include "messaging/Identity.hh"
#include "Blob.hh"

#include <boost/iterator/transform_iterator.hpp>

#include <functional>
#include <vector>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Interface for handling messages
 *
 * BasicMessageHandler is an interface for a driver (MessageQueue object) for
 * handling a message sent by a client or peer, and generating the reply for
 * them.
 *
 * The driver is responsible for providing identity of the sender and any
 * arguments accompanying the message to the MessageHandler implementation. The
 * MessageHandler implementation then uses the sink provided by the driver to
 * communicate the reply parts.
 *
 * A BasicMessageHandler supports a pluggable execution policy which controls
 * how the message handler interacts with its execution context. The simples
 * execution policy is SynchronousExecutionPolicy which is used for message
 * handlers that are executed synchronously in their drivers call
 * stack. MessageHandler is an alias for a BasicMessageHandler with synchronous
 * execution policy.
 *
 * \tparam ExecutionPolicy the exexution policy
 */
template<typename ExecutionPolicy>
class BasicMessageHandler {
public:

    /** \brief Output sink the MessageHandler writes its reply arguments to
     */
    using OutputSink = std::function<void(ByteSpan)>;

    virtual ~BasicMessageHandler() = default;

    /** \brief Handle message
     *
     * \tparam ParameterIterator An input iterator to the sequence of arguments
     * to the message. Each parameter is a view to a contiguous sequence of
     * bytes whose interpretation is left to the MessageHandler object.
     *
     * \param execution the execution context
     * \param identity the identity of the sender of the message
     * \param first iterator to the first parameter of the message
     * \param last iterator one past the last parameter of the message
     * \param out function to be invoked once for each reply parameter of the
     * message
     *
     * \return true if the message was handled successfully, false otherwise
     */
    template<typename ParameterIterator>
    bool handle(
        ExecutionPolicy& execution, const Identity& identity,
        ParameterIterator first, ParameterIterator last, OutputSink out);

protected:

    /** \brief Input parameter to doHandle()
     */
    using ParameterVector = std::vector<ByteSpan>;

private:

    /** \brief Handle action of this handler
     *
     * \param execution the execution context
     * \param identity the identity of the sender of the message
     * \param params vector containing the parameters of the message
     * \param sink
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \sa handle()
     */
    virtual bool doHandle(
        ExecutionPolicy& execution, const Identity& identity,
        const ParameterVector& params, OutputSink sink) = 0;
};

template<typename ExecutionPolicy>
template<typename ParameterIterator>
bool BasicMessageHandler<ExecutionPolicy>::handle(
    ExecutionPolicy& execution, const Identity& identity,
    ParameterIterator first, ParameterIterator last, OutputSink out)
{
    const auto to_bytes = [](const auto& p) { return asBytes(p); };
    return doHandle(
        execution, identity,
        ParameterVector(
            boost::make_transform_iterator(first, to_bytes),
            boost::make_transform_iterator(last, to_bytes)),
        std::move(out));
}

/** \brief Synchronous execution policy for message handlers
 *
 * Provides no services for execution context. A synchronous message handler
 * just handles the message in the call stack of the driver and returns.
 *
 * \sa BasicMessageHandler, MessageHandler
 */
struct SynchronousExecutionPolicy {};

/** \brief Message handler with synchronous execution policy
 */
using MessageHandler = BasicMessageHandler<SynchronousExecutionPolicy>;

}
}

#endif // MESSAGING_MESSAGEHANDLER_HH_
