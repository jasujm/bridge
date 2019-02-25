#ifndef COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_
#define COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_

#include "coroutines/CoroutineAdapter.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/Poller.hh"
#include "Utility.hh"

namespace Bridge {
namespace Coroutines {

/** \brief Asynchronous execution policy for message handlers
 *
 * Executes a callback as a coroutine.
 *
 * \sa BasicMessageHandler, AsynchronousMessageHandler
 */
struct AsynchronousExecutionPolicy {
public:

    /** \Brief Create asynchronous execution policy
     *
     * \param poller the poller used for polling the sockets the coroutine is
     * awaiting
     */
    explicit AsynchronousExecutionPolicy(Messaging::Poller& poller);

    /** \brief Execute callback as coroutine
     *
     * \param callback the callback to execute
     */
    template<typename Callback>
    void operator()(Callback&& callback);

    /** \param Await socket
     *
     * \param socket the socket to await
     */
    void await(CoroutineAdapter::AwaitableSocket socket);

private:

    CoroutineAdapter::Sink* sink;
    Messaging::Poller* poller;
};

template<typename Callback>
void AsynchronousExecutionPolicy::operator()(Callback&& callback)
{
    assert(poller);
    CoroutineAdapter::create(
        [*this, callback = std::forward<Callback>(callback)](auto& sink) mutable
        {
            this->sink = &sink;
            std::invoke(std::move(callback), *this);
        }, *poller);
}

/** \brief Message handler with asynchronous execution policy
 */
using AsynchronousMessageHandler =
    Messaging::BasicMessageHandler<AsynchronousExecutionPolicy>;

}
}

#endif // COROUTINES_ASYNCHRONOUSEXECUTIONPOLICY_HH_
