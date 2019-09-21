/** \file
 *
 * \brief Definition of Bridge::Messaging::TerminationGuard
 */

#ifndef MESSAGING_TERMINATIONGUARD_HH_
#define MESSAGING_TERMINATIONGUARD_HH_

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
    static Messaging::Socket createTerminationSubscriber(Messaging::MessageContext& context);

    /** \brief Create new termination guard
     *
     * \param context The ZeroMQ context
     */
    TerminationGuard(Messaging::MessageContext& context);

    /** \brief Publish termination notification
     */
    ~TerminationGuard();

private:

    static inline const std::string ENDPOINT {
        "inproc://bridge.terminationguard"};

    Messaging::Socket terminationPublisher;
};

inline Messaging::Socket
TerminationGuard::createTerminationSubscriber(Messaging::MessageContext& context)
{
    auto socket = Messaging::Socket {context, Messaging::SocketType::sub};
    socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    socket.connect(ENDPOINT);
    return socket;
}

inline TerminationGuard::TerminationGuard(Messaging::MessageContext& context) :
    terminationPublisher {context, Messaging::SocketType::pub}
{
    terminationPublisher.bind(ENDPOINT);
}

inline TerminationGuard::~TerminationGuard()
{
    terminationPublisher.send("", 0);
}

}
}

#endif // MESSAGING_TERMINATIONGUARD_HH_
