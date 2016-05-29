/** \file
 *
 * \brief Definition of Bridge::Main::PeerCommandSender
 */

#ifndef MAIN_PEERCOMMANDSENDER_HH_
#define MAIN_PEERCOMMANDSENDER_HH_

#include "messaging/SerializationUtility.hh"

#include <boost/core/noncopyable.hpp>
#include <zmq.hpp>

#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <queue>
#include <utility>

namespace Bridge {
namespace Main {

/** \brief Reliably send commands to peers
 *
 * A PeerCommandSender object has a queue of commands that are to be sent to
 * all peers. It monitors replies from the peers and tries to resend the
 * command to peers who reply failure. The assumption is that failure is
 * caused by temporary out‐of‐sync state between the peers (maybe because a
 * peer has not yet processed earlier commands from other peers) that will
 * eventually resolve given that all peers have correcly implemented the
 * protocol.
 */
class PeerCommandSender : private boost::noncopyable {
public:

    /** \brief Add peer to PeerCommandSender
     *
     * sendToAll() automatically sends message to all peers added with this
     * method.
     *
     * \param socket the socket associated with the peer
     */
    void addPeer(zmq::socket_t& socket);

    /** \brief Send command to all peers
     *
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
     * \throw std::out_of_range if socket was not added using addSocket()
     */
    void processReply(zmq::socket_t& socket);

private:

    using Message = std::vector<std::string>;

    void internalSendMessage(zmq::socket_t& socket);
    void internalSendMessageToAll();
    void internalAddMessage(Message message);

    std::queue<Message> messages;
    std::map<zmq::socket_t*, bool> peers;
};

template<
    typename SerializationPolicy, typename CommandString, typename... Params>
void PeerCommandSender::sendCommand(
    SerializationPolicy&& serializer, CommandString&& command,
    Params&&... params)
{
    constexpr auto count = 1u + sizeof...(params);
    auto message = Message(count);
    message[0] = std::forward<CommandString>(command);
    serializeAll(
        std::next(message.begin()),
        std::forward<SerializationPolicy>(serializer),
        std::forward<Params>(params)...);
    internalAddMessage(std::move(message));
}

}
}

#endif // MAIN_PEERCOMMANDSENDER_HH_
