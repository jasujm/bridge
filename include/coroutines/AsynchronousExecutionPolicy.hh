/** \file
 *
 * \brief Definition of Bridge::Coroutines::AsynchronousExecutionPolicy
 */

#ifndef COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_
#define COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_

#include "coroutines/CoroutineAdapter.hh"
#include "messaging/MessageHandler.hh"

#include <cassert>

namespace Bridge {

namespace Messaging {

class Poller;

}

namespace Coroutines {

/** \brief Asynchronous execution context
 *
 * The context holds reference to the coroutine sink that can be used to
 * await sockets.
 */
class AsynchronousExecutionContext {
public:

    /** \brief Create asynchronous execution context
     *
     * \param sink the coroutine sink
     */
    explicit AsynchronousExecutionContext(CoroutineAdapter::Sink& sink);

    /** \brief Await socket
     *
     * Suspend the coroutine until \p socket becomes readable.
     *
     * \param socket the socket to await
     */
    void await(std::shared_ptr<zmq::socket_t> socket);

private:

    CoroutineAdapter::Sink* sink;
};

/** \brief Asynchronous execution policy
 *
 * Asynchronous execution policy creates a coroutine, and executes a function in
 * the coroutine context. The caller resumes when the coroutine completes or
 * awaits for an event.
 *
 * \sa Messaging::BasicMessageHandler, AsynchronousMessageHandler
 */
struct AsynchronousExecutionPolicy {
public:

    /** \brief Asynchronous execution context
     */
    using Context = AsynchronousExecutionContext;

    /** \brief Create asynchronous execution policy
     *
     * \param poller the poller used for polling the sockets the coroutine is
     * awaiting
     */
    explicit AsynchronousExecutionPolicy(Messaging::Poller& poller);

    /** \brief Execute callback as coroutine
     *
     * Creates new coroutine, and invokes \p callback in the coroutine
     * context. The argument for the callback is a Context object that can be
     * used to await events.
     *
     * \param callback the callback to be executed
     */
    template<typename Callback>
    void operator()(Callback&& callback);

private:

    Messaging::Poller* poller;
};

template<typename Callback>
void AsynchronousExecutionPolicy::operator()(Callback&& callback)
{
    assert(poller);
    CoroutineAdapter::create(
        [*this, callback = std::forward<Callback>(callback)](auto& sink) mutable
        {
            std::invoke(std::move(callback), Context {sink});
        }, *poller);
}

/** \brief Message handler with asynchronous execution policy
 */
using AsynchronousMessageHandler =
    Messaging::BasicMessageHandler<AsynchronousExecutionPolicy>;

/** \brief Await \p socket
 *
 * If \p socket is already readable (calling \c recv() on it does not block)
 * does nothing. Otherwise calls AsynchronousExecutioContext::await() on \p
 * context, in order to await \p socket.
 *
 * \note Using this function call instead invoking \p context.await() directly
 * allows writing generic code that can wait for a socket to become readable
 * with any execution policy.
 *
 * \param context the execution context
 * \param socket the socket to await
 */
void ensureSocketReadable(
    AsynchronousExecutionContext& context,
    std::shared_ptr<zmq::socket_t> socket);

}
}

#endif // COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_
