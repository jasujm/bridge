/** \file
 *
 * \brief Definition of Bridge::Messaging::BasicMessageHandler interface
 */

#ifndef MESSAGING_MESSAGEHANDLER_HH_
#define MESSAGING_MESSAGEHANDLER_HH_

#include "messaging/Identity.hh"
#include "messaging/SynchronousExecutionPolicy.hh"
#include "Blob.hh"

#include <boost/iterator/transform_iterator.hpp>

#include <functional>
#include <vector>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief MessageHandler response collector
 *
 * Response is an interface for a MessageHandler object to communicate the
 * response of an invocation of the handler to its driver. A response consists
 * of numeric status and zero or more response frames.
 */
class Response {
public:

    virtual ~Response();

    /** \brief Set the status of the response
     *
     * \param status the status
     */
    void setStatus(ByteSpan status);

    /** \brief Add another frame to the response
     *
     * \param frame the next frame
     */
    void addFrame(ByteSpan frame);

private:

    /** \brief Handle for setStatus()
     */
    virtual void handleSetStatus(ByteSpan status) = 0;

    /** \brief Handle for addFrame()
     */
    virtual void handleAddFrame(ByteSpan frame) = 0;
};

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
 * how the message handler interacts with its execution context. The simplest
 * execution policy is SynchronousExecutionPolicy which is used for message
 * handlers that are executed synchronously in their driver’s call
 * stack. MessageHandler is an alias for a BasicMessageHandler with synchronous
 * execution policy.
 *
 * \tparam ExecutionPolicy A type satisfying the \ref executionpolicy
 */
template<typename ExecutionPolicy>
class BasicMessageHandler {
public:

    /** \brief The execution policy used by the message handler
     */
    using ExecutionPolicyType = ExecutionPolicy;

    /** \brief Context the message handler is executed in
     */
    using ExecutionContext = typename ExecutionPolicy::Context;

    virtual ~BasicMessageHandler() = default;

    /** \brief Handle message
     *
     * \tparam ParameterIterator An input iterator to the sequence of arguments
     * to the message. Each parameter is a view to a contiguous sequence of
     * bytes whose interpretation is left to the MessageHandler object.
     *
     * \param context the execution context
     * \param identity the identity of the sender of the message
     * \param first iterator to the first parameter of the message
     * \param last iterator one past the last parameter of the message
     * \param response the response object used to communicate with the driver
     *
     * \return true if the message was handled successfully, false otherwise
     */
    template<typename ParameterIterator>
    void handle(
        ExecutionContext context, const Identity& identity,
        ParameterIterator first, ParameterIterator last, Response& response);

protected:

    /** \brief Input parameter to doHandle()
     */
    using ParameterVector = std::vector<ByteSpan>;

private:

    /** \brief Handle action of this handler
     *
     * \param context the execution context
     * \param identity the identity of the sender of the message
     * \param params vector containing the parameters of the message
     * \param respones the response object
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \sa handle()
     */
    virtual void doHandle(
        ExecutionContext context, const Identity& identity,
        const ParameterVector& params, Response& response) = 0;
};

template<typename ExecutionPolicy>
template<typename ParameterIterator>
void BasicMessageHandler<ExecutionPolicy>::handle(
    ExecutionContext context, const Identity& identity,
    ParameterIterator first, ParameterIterator last, Response& response)
{
    const auto to_bytes = [](const auto& p) { return asBytes(p); };
    doHandle(
        std::move(context), identity,
        ParameterVector(
            boost::make_transform_iterator(first, to_bytes),
            boost::make_transform_iterator(last, to_bytes)),
        response);
}

/** \brief Message handler with synchronous execution policy
 */
using MessageHandler = BasicMessageHandler<SynchronousExecutionPolicy>;

}
}

#endif // MESSAGING_MESSAGEHANDLER_HH_
