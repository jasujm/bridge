/** \file
 *
 * \brief Definition of Bridge::Messaging::TerminationGuard
 */

#ifndef MESSAGING_TERMINATIONGUARD_HH_
#define MESSAGING_TERMINATIONGUARD_HH_

#include "messaging/MessageUtility.hh"
#include "messaging/Sockets.hh"

#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Termination publisher for unit tests
 *
 * This class simulates the feature of Bridge::Messaging::MessageLoop to publish
 * termination message. The messaging framework uses the feature to notify
 * worker threads when they should terminate. However, configuring a full
 * message loop is unnecessary in unit testing, so this minimal guard class can
 * be used.
 *
 * There is one global endpoint for the pub–sub pairs. Only one TerminationGuard
 * object should exist at one time in a given ZeroMQ context.
 */
class TerminationGuard {
public:

    /** \brief Create termination subscriber
     *
     * \note This function is static, enabling termination subscribers to be
     * created before the TerminationGuard object.
     *
     * \param context The ZeroMQ context
     *
     * \return
     */
    static Socket createTerminationSubscriber(MessageContext& context);

    /** \brief Create new termination guard
     *
     * \param context The ZeroMQ context
     */
    TerminationGuard(MessageContext& context);

    /** \brief Publish termination notification
     */
    ~TerminationGuard();

private:

    static constexpr auto ENDPOINT = std::string_view {
        "inproc://bridge.terminationguard"};

    Socket terminationPublisher;
};

inline Socket
TerminationGuard::createTerminationSubscriber(MessageContext& context)
{
    auto socket = Socket {context, SocketType::sub};
    socket.set(zmq::sockopt::subscribe, "");
    connectSocket(socket, ENDPOINT);
    return socket;
}

inline TerminationGuard::TerminationGuard(MessageContext& context) :
    terminationPublisher {context, SocketType::pub}
{
    bindSocket(terminationPublisher, ENDPOINT);
}

inline TerminationGuard::~TerminationGuard()
{
    sendEmptyMessage(terminationPublisher);
}

}
}

#endif // MESSAGING_TERMINATIONGUARD_HH_
