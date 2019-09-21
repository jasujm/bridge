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

    /** \brief Suspend coroutine on \p awaitable
     *
     * \param awaitable an object that can be awaited by the coroutine
     */
    template<typename Awaitable>
    void await(Awaitable&& awaitable);

private:

    CoroutineAdapter::Sink* sink;
};

template<typename Awaitable>
void AsynchronousExecutionContext::await(Awaitable&& awaitable)
{
    assert(sink);
    (*sink)(std::forward<Awaitable>(awaitable));
}

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
     * \param callbackScheduler callback scheduler
     */
    AsynchronousExecutionPolicy(
        std::weak_ptr<Messaging::Poller> poller,
        std::weak_ptr<Messaging::CallbackScheduler> callbackScheduler);

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

    std::weak_ptr<Messaging::Poller> poller;
    std::weak_ptr<Messaging::CallbackScheduler> callbackScheduler;
};

template<typename Callback>
void AsynchronousExecutionPolicy::operator()(Callback&& callback)
{
    CoroutineAdapter::create(
        [this, callback = std::forward<Callback>(callback)](auto& sink) mutable
        {
            std::invoke(std::move(callback), Context {sink});
        }, poller, callbackScheduler);
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
    AsynchronousExecutionContext& context, Messaging::SharedSocket socket);

}
}

#endif // COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_
