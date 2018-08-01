/** \file
 *
 * \brief Definition of Bridge::Main::PeerCommandSender
 */

#ifndef MAIN_PEERCOMMANDSENDER_HH_
#define MAIN_PEERCOMMANDSENDER_HH_

#include "messaging/CommandUtility.hh"
#include "messaging/Security.hh"

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

#include <chrono>
#include <iterator>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace Bridge {
namespace Main {

class CallbackScheduler;

/** \brief Reliably send commands to peers
 *
 * A PeerCommandSender object has a queue of commands that are to be sent to all
 * peers. It monitors replies from the peers. If a peer replies failure, the
 * peer command sender tries to resend the command to peers with increasingly
 * long intervals. The assumption is that failure is caused by temporary
 * out‐of‐sync state between the peers (maybe because a peer has not yet
 * processed earlier commands from other peers) that will eventually resolve
 * given that all peers have correcly implemented the protocol.
 */
class PeerCommandSender : private boost::noncopyable {
public:

    /** \brief Create peer command sender
     *
     * \param callbackScheduler callback scheduler for the message loop
     */
    PeerCommandSender(std::shared_ptr<CallbackScheduler> callbackScheduler);

    /** \brief Create peer
     *
     * The method creates a new DEALER socket that is connected to \p
     * endpoint. sendCommand() can be used to send command to all peers
     * created using this method.
     *
     * \param context the ZeroMQ context of the new socket
     * \param keys the CurveZMQ keys used for connections, or nullptr if curve
     * security isn’t used
     * \param endpoint the endpoint of the peer
     *
     * \return the socket created by the method
     */
    std::shared_ptr<zmq::socket_t> addPeer(
        zmq::context_t& context, const Messaging::CurveKeys* keys,
        const std::string& endpoint);

    /** \brief Send command to all peers
     *
     * The command will be sent to all peers created earlier using addPeer().
     * If there is a previous command that all peers have not yet replied to,
     * the message is put to queue until successful replies have been received
     * from all peers to the previous messages.
     *
     * \param serializer serialization policy for the command parameters
     * \param command the command sent as the first part of the message
     * \param params the parameters serialized and sent as the subsequent
     * parts of the message
     *
     * \sa \ref serializationpolicy
     */
    template<
        typename SerializationPolicy, typename CommandString,
        typename... Params>
    void sendCommand(
        SerializationPolicy&& serializer, CommandString&& command,
        Params&&... params);

    /** \brief Receive and process reply from socket
     *
     * This method receives message from \p socket and examines whether or not
     * it is a successful reply. If not, the command is resent.
     *
     * \throw std::invalid_argument if socket is not one added using addPeer()
     */
    void processReply(zmq::socket_t& socket);

private:

    using Message = std::vector<std::string>;

    struct Peer {
        Peer(
            zmq::context_t& context, const Messaging::CurveKeys* keys,
            const std::string& endpoint);
        std::shared_ptr<zmq::socket_t> socket;
        std::chrono::milliseconds resendTimeout;
        bool success;
    };

    void internalSendMessage(zmq::socket_t& socket);
    void internalSendMessageToAll();
    void internalAddMessage(Message message);

    std::shared_ptr<CallbackScheduler> callbackScheduler;
    std::queue<Message> messages;
    std::vector<Peer> peers;
};

template<
    typename SerializationPolicy, typename CommandString, typename... Params>
void PeerCommandSender::sendCommand(
    SerializationPolicy&& serializer, CommandString&& command,
    Params&&... params)
{
    if (!peers.empty()) {
        constexpr auto count = 1u + 2 * sizeof...(params);
        auto message = Message(count);
        makeCommand(
            message.begin(),
            std::forward<SerializationPolicy>(serializer),
            std::forward<CommandString>(command),
            std::forward<Params>(params)...);
        internalAddMessage(std::move(message));
    }
}

}
}

#endif // MAIN_PEERCOMMANDSENDER_HH_
