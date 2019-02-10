/** \file
 *
 * \brief Definition of Bridge::Messaging::CoroutineAdapter
 */

#ifndef COROUTINES_COROUTINEADAPTER_HH_
#define COROUTINES_COROUTINEADAPTER_HH_

#include "messaging/Poller.hh"

#include <boost/coroutine2/all.hpp>
#include <zmq.hpp>

#include <functional>
#include <memory>

namespace Bridge {

/** \brief Classes for executing coroutines in the bridge framework
 */
namespace Coroutines {

/** \brief Adapt socket based coroutine into the message loop framework
 *
 * This class works as glue between a message loop polling ZeroMQ sockets and
 * Boost.Coroutine (https://theboostcpplibraries.com/boost.coroutine) based
 * coroutines. More specifically it allows creating push coroutines that can
 * await sockets by pushing them to the main context where (presumably) a
 * message loop polls the socket and returns context to the coroutine once the
 * socket becomes readable.
 *
 * \todo Mechanism to cancel a coroutine cleanly is needed.
 *
 * \sa create() for documentation about creating a coroutine and expectations
 * for a coroutine function
 */
class CoroutineAdapter :
    public std::enable_shared_from_this<CoroutineAdapter> {
public:

    struct DoNotCallDirectly {};

    /** \brief Socket that a coroutine can await
     */
    using AwaitableSocket = std::shared_ptr<zmq::socket_t>;

    /** \brief Sink used by a coroutine function to await a socket
     */
    using Sink = boost::coroutines2::coroutine<AwaitableSocket>::push_type;

    /** \brief Create new coroutine adapter
     *
     * The coroutine starts executing immediately, until it awaits a socket by
     * pushing it to the sink or completes.
     *
     * A CoroutineFunction accepts one parameter, a sink that accepts an
     * instance of AwaitableSocket. The function can, by pushing a socket to the
     * sink, signal that it wants to await the socket to become readable.
     *
     * The behavior is undefined if a coroutine function ever pushes a \c
     * nullptr to the sink.
     *
     * \param coroutine the coroutine function associated with the coroutine
     * adapter
     * \param poller the poller used for polling the sockets for the coroutine
     *
     * \throw Any exception thrown in the coroutine function
     *
     * \sa CoroutineFunction for the expectations of the coroutine function
     */
    template<typename CoroutineFunction>
    static std::shared_ptr<CoroutineAdapter> create(
        CoroutineFunction&& coroutine, Messaging::Poller& poller);

    /** \brief Constructor
     *
     * CoroutineAdapter is intended to be managed by shared pointer This
     * constructor should not be invoked directly, but an instance should be
     * created using create().
     */
    template<typename CoroutineFunction>
    CoroutineAdapter(
        DoNotCallDirectly, CoroutineFunction&& coroutine, Messaging::Poller&);

    /** \brief Return the awaited socket, if any
     *
     * \return The socket the coroutine is awaiting, or nullptr if the coroutine
     * has completed.
     */
    AwaitableSocket getAwaitedSocket() const;

    /** \brief Transfer control to the coroutine
     *
     * This function checks if \p socket was the one the coroutine was awaiting,
     * and transfers the control to the coroutine if it is. Otherwise throws an
     * exception.
     *
     * \param socket the awaited socket
     *
     * \throw std::invalid_argument if the coroutine was not awaiting \p socket
     * \throw Any exception thrown in the coroutine function
     */
    void operator()(zmq::socket_t& socket);

private:

    void internalUpdateSocket();

    using Source = boost::coroutines2::coroutine<AwaitableSocket>::pull_type;

    Source source;
    AwaitableSocket awaitedSocket;
    Messaging::Poller& poller;
};

template<typename CoroutineFunction>
std::shared_ptr<CoroutineAdapter> CoroutineAdapter::create(
    CoroutineFunction&& coroutine, Messaging::Poller& poller)
{
    auto adapter = std::make_shared<CoroutineAdapter>(
        DoNotCallDirectly {}, std::forward<CoroutineFunction>(coroutine),
        poller);
    adapter->internalUpdateSocket();
    return adapter;
}

template<typename CoroutineFunction>
CoroutineAdapter::CoroutineAdapter(
    DoNotCallDirectly, CoroutineFunction&& coroutine, Messaging::Poller& poller) :
    source {std::forward<CoroutineFunction>(coroutine)},
    poller {poller}
{
}

}
}

#endif // COROUTINES_COROUTINEADAPTER_HH_
