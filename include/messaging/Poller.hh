/** \file
 *
 * \brief Definition of Bridge::Messaging::Poller
 */

#ifndef MESSAGING_POLLER_HH_
#define MESSAGING_POLLER_HH_

#include "messaging/Sockets.hh"

#include <functional>

namespace Bridge {
namespace Messaging {

/** \brief Interface for socket poller
 *
 * Poller is an interface for a message loop that can be used to invoke
 * callbacks when a socket becomes readable.
 */
class Poller {
public:

    /** \brief Socket that can be registered with the poller
     */
    using PollableSocket = SharedSocket;

    /** \brief Callback that can be registered with the poller
     */
    using SocketCallback = std::function<void(Socket&)>;

    virtual ~Poller();

    /** \brief Register pollable socket–callback pair
     *
     * Start polling \p socket and invoke \p callback whenever there is a
     * message to be received from \p socket. It is assumed that all sockets
     * registered via this interface share the same context.
     *
     * \note The \p socket is accepted as shared pointer and stored until
     * removeSocket() is called on the same socket, or the poller is
     * destructed. It is the responsibility of the client to ensure that no
     * pointer or reference captured by \p callback becomes dangling while the
     * poller holds the callback.
     *
     * \param socket the socket to poll
     * \param callback the callback to invoke when \p socket becomes readable
     *
     * \throw std::invalid_argument if either \p socket or \p callback is empty
     */
    void addPollable(PollableSocket socket, SocketCallback callback);

    /** \brief Remove pollable socket–callback pair
     *
     * After calling removePollable() on \p socket, the poller deletes its
     * PollableSocket instance holding reference to \p socket, and the
     * associated callback.
     *
     * \param socket the socket to deregister
     */
    void removePollable(Socket& socket);

private:

    /** \brief Handle for addPollable()
     *
     * The implementation may assume that \p socket and \p callback are not
     * empty.
     *
     * \param socket the socket to poll
     * \param callback the callback to invoke when \p socket becomes readable
     */
    virtual void handleAddPollable(
        PollableSocket socket, SocketCallback callback) = 0;

    /** \brief Handle for removePollable()
     *
     * \param socket the socket to deregister
     */
    virtual void handleRemovePollable(Socket& socket) = 0;

};

}
}

#endif // MESSAGING_POLLER_HH_
