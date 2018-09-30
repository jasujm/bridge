/** \file
 *
 * \brief Definition of Bridge::Main::PeerCommandSender
 */

#ifndef MAIN_PEERCOMMANDSENDER_HH_
#define MAIN_PEERCOMMANDSENDER_HH_

#include "messaging/CommandUtility.hh"
#include "messaging/Security.hh"

#include <boost/core/noncopyable.hpp>
#include <boost/iterator/transform_iterator.hpp>
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
     * \param endpoint the endpoint of the peer
     * \param keys the CurveZMQ keys used for connections, or nullptr if the
     * curve security isn’t used
     * \param serverKey the CurveZMQ server key for the remote peer, or
     * empty if the curve security isn’t used
     *
     * \return the socket created by the method
     */
    std::shared_ptr<zmq::socket_t> addPeer(
        zmq::context_t& context, const std::string& endpoint,
        const Messaging::CurveKeys* keys = nullptr, ByteSpan serverKey = {});

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
     *
     * \deprecated The preferred way to integrate to message loop is by using
     * getSockets()
     */
    void processReply(zmq::socket_t& socket);

    /** \brief Get socket–callback pairs for handling replies
     *
     * This method generates a range of pairs containing ZMQ sockets and
     * callbacks handling reply from that socket. The intention is to register
     * all pairs returned by the method to a Messaging::MessageLoop object to
     * ensure the proper functioning of the callback scheduler object.
     *
     * \return Range containing socket–callback pairs
     */
    auto getSockets();

private:

    using Message = std::vector<zmq::message_t>;

    struct Peer {
        Peer(
            zmq::context_t& context, const std::string& endpoint,
            const Messaging::CurveKeys* keys, ByteSpan serverKey);
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
        auto message = Message {};
        message.reserve(count);
        Messaging::makeCommand(
            std::back_inserter(message),
            std::forward<SerializationPolicy>(serializer),
            std::forward<CommandString>(command),
            std::forward<Params>(params)...);
        internalAddMessage(std::move(message));
    }
}

inline auto PeerCommandSender::getSockets()
{
    auto callback = [this](auto& socket) { processReply(socket); };
    auto create_socket_cb_pair = [&callback](const auto& peer)
    {
        return std::pair {peer.socket, callback};
    };
    return std::vector<decltype(create_socket_cb_pair(peers.front()))>(
        boost::make_transform_iterator(peers.begin(), create_socket_cb_pair),
        boost::make_transform_iterator(peers.end(), create_socket_cb_pair));
}

}
}

#endif // MAIN_PEERCOMMANDSENDER_HH_
