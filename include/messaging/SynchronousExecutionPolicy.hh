/** \file
 *
 * \brief Definition of Bridge::Messaging::SynchronousExecutionPolicy
 *
 * \page executionpolicy ExecutionPolicy concept
 *
 * Bridge::Messaging::BasicMessageHandler class uses classes satisfying the
 * execution policy concept to control how they are executed (synchronously,
 * asynchronously, in a worker thread etc.). Given execution policy \c e of
 * execution policy type \c E, and a callable \c f, then the expression \c e(f)
 * shall
 *
 * 1. Create the execution context for executing the code encapsulated by \c c,
 *    if necessary
 * 2. Invoke \c f with argument \c c, that is a rvalue of type \c E::Context
 *
 * The intention is that \c c encapsulates the necessary handle for the code
 * executing in \c f to interact with its execution context. Unless \c E
 * describes synchronous execution, the invocation of \c e may complete
 * independently from the invocation of \c f (see
 * e.g. Bridge::Coroutines::AsynchronousExecutionPolicy).
 */

#ifndef MESSAGING_SYNCHRONOUSEXECUTIONPOLICY_HH_
#define MESSAGING_SYNCHRONOUSEXECUTIONPOLICY_HH_

#include "Utility.hh"

#include <zmq.hpp>

#include <memory>
#include <functional>

namespace Bridge {
namespace Messaging {

/** \brief Dummy context
 */
struct SynchronousExecutionContext {};

/** \brief Synchronous execution policy
 *
 * Synchronous execution policy simply executes a function directly in its
 * caller’s call stack. The caller resumes only after the executed function
 * returns.
 *
 * \sa BasicMessageHandler, MessageHandler
 */
class SynchronousExecutionPolicy {
public:

    /** \brief Synchronous execution context
     */
    using Context = SynchronousExecutionContext;

    /** \brief Execute callback synchronously
     *
     * Invokes \p callback, passing Context object as its argument
     *
     * \param callback the callback to be executed
     */
    template<typename Callback>
    void operator()(Callback&& callback);
};

template<typename Callback>
void SynchronousExecutionPolicy::operator()(Callback&& callback)
{
    std::invoke(std::forward<Callback>(callback), Context {});
}

/** \brief Ensure \p socket is readable
 *
 * If \p socket is already readable (calling \c recv() on it does not block)
 * does nothing. Otherwise calls polls \p socket until it becomes readable.
 *
 * \note This function and its overloads exists to allow writing generic code
 * that can ensure readability of a socket in different execution
 * contexts.
 *
 * \param context the execution context
 * \param socket the socket to poll
 */
void ensureSocketReadable(
    SynchronousExecutionContext& context,
    std::shared_ptr<zmq::socket_t> socket);

}
}

#endif // MESSAGING_SYNCHRONOUSEXECUTIONPOLICY_HH_
