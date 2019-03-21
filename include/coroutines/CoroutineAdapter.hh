/** \file
 *
 * \brief Definition of Bridge::Coroutines::CoroutineAdapter
 */

#ifndef COROUTINES_COROUTINEADAPTER_HH_
#define COROUTINES_COROUTINEADAPTER_HH_

#include "messaging/Poller.hh"

#include <boost/coroutine2/all.hpp>
#include <zmq.hpp>

#include <functional>
#include <memory>
#include <variant>

namespace Bridge {

/** \brief Classes for executing coroutines in the bridge framework
 */
namespace Coroutines {

class CoroutineAdapter;

/** \brief Future that a coroutine can await
 *
 * When awaited by CoroutineAdapter, the coroutine will be resumed when the
 * future is completed by another (co)routine.
 *
 * \note The class is so named because it resembles the well‐known future
 * concept. As of now only “void” futures are supported, i.e. the coroutine
 * can be notified but no value can be transferred using it.
 */
class Future
{
public:

    /** \brief Default constructor
     *
     * Create a future that is initially not awaited by any coroutine.
     */
    Future();

    /** \brief Resolve the future
     *
     * If a coroutine function associated with a \ref CoroutineAdapter was
     * awaiting this future, it is resumed.
     */
    void resolve();

private:

    std::function<void()> resolveCallback;

    friend class CoroutineAdapter;
};

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

    /** \brief Awaitable object
     */
    using Awaitable = std::variant<
        std::shared_ptr<Future>,
        std::shared_ptr<zmq::socket_t>>;

    /** \brief Sink used by a coroutine function to await a socket
     *
     * A coroutine function used with a coroutine adapter object receives an
     * instance of Sink as its only parameter. The sink type allows the
     * coroutine to await events, represented by one of the alternatives of \ref
     * Awaitable variant.
     *
     * If the pushed object is a ZeroMQ socket, the coroutine is suspended until
     * the socket becomes readable. The behavior is undefined if a coroutine
     * function pushes a \c nullptr to the sink.
     *
     * If the object is a \ref Future, the coroutine is suspended until the
     * future is completed. When pushing a future to the sink, it should \e not
     * be moved, because the original future would be left in an unspecified
     * state not supporting resolution.

     */
    using Sink = boost::coroutines2::coroutine<Awaitable>::push_type;

    /** \brief Create new coroutine adapter
     *
     * The coroutine starts executing immediately, until it awaits a socket by
     * pushing it to the sink or completes.
     *
     * A coroutine function accepts one parameter, a \ref Sink object that
     * accepts an instance of \ref Awaitable. The function can, by pushing an
     * awaitable object to the sink, signal that it wants to suspend until an
     * event takes place.
     *
     * \param coroutine the coroutine function associated with the coroutine
     * adapter
     * \param poller the poller used for polling the sockets for the coroutine
     *
     * \throw Any exception thrown in the coroutine function
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

    /** \brief Return the awaited object, if any
     *
     * \return Pointer to the \ref Awaitable object the coroutine is awaiting,
     * or nullptr if the coroutine has completed. The pointer remains valid
     * while the coroutine is suspended, awaiting for the returned object.
     */
    const Awaitable* getAwaited() const;

private:

    class ClearAwaitVisitor;
    class PrepareAwaitVisitor;

    void internalResume();
    void internalUpdate();

    using Source = boost::coroutines2::coroutine<Awaitable>::pull_type;

    Source source;
    Awaitable awaited;
    Messaging::Poller& poller;
};

template<typename CoroutineFunction>
std::shared_ptr<CoroutineAdapter> CoroutineAdapter::create(
    CoroutineFunction&& coroutine, Messaging::Poller& poller)
{
    auto adapter = std::make_shared<CoroutineAdapter>(
        DoNotCallDirectly {}, std::forward<CoroutineFunction>(coroutine),
        poller);
    adapter->internalUpdate();
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
