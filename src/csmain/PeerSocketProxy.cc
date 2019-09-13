#include "csmain/PeerSocketProxy.hh"

#include "messaging/Identity.hh"
#include "messaging/MessageUtility.hh"
#include "Utility.hh"

#include <boost/endian/conversion.hpp>
#include <boost/range/combine.hpp>
#include <boost/format.hpp>

#include <cassert>
#include <cstring>

namespace Bridge {
namespace CardServer {

PeerSocketProxy::PeerSocketProxy(
    zmq::context_t& context, zmq::socket_t peerServerSocket,
    std::vector<zmq::socket_t> peerClientSockets,
    const int selfOrder, AuthorizationFunction authorizer) :
    selfOrder {static_cast<OrderParameter>(selfOrder)},
    authorizer {std::move(authorizer)},
    peerServerSocket {
        std::make_shared<zmq::socket_t>(std::move(peerServerSocket))}
{
    const auto n_peer_sockets = peerClientSockets.size();
    peerClientSockets.reserve(n_peer_sockets);
    frontStreamSockets.reserve(n_peer_sockets);
    streamSockets.reserve(n_peer_sockets);
    for (const auto n : to(n_peer_sockets)) {
        this->peerClientSockets.emplace_back(
            std::make_shared<zmq::socket_t>(
                std::move(peerClientSockets[n])));
        const auto endpoint = boost::format(
            "inproc://bridge.cardserver.peersocketproxy.%1%.peer%2%")
            % this % n;
        const auto endpoint_str = endpoint.str();
        auto front_socket = std::make_shared<zmq::socket_t>(
            context, zmq::socket_type::pair);
        front_socket->connect(endpoint_str);
        frontStreamSockets.emplace_back(std::move(front_socket));
        auto back_socket = std::make_shared<zmq::socket_t>(
            context, zmq::socket_type::pair);
        back_socket->bind(endpoint_str);
        streamSockets.emplace_back(std::move(back_socket));
    }
}

PeerSocketProxy::SocketCallbackVector PeerSocketProxy::getPollables()
{
    auto ret = SocketCallbackVector {
        {
            peerServerSocket,
            [this](auto& socket) { internalHandleMessageFromPeer(socket); }
        }
    };
    for (auto&& t : boost::combine(frontStreamSockets, peerClientSockets)) {
        ret.emplace_back(
            t.get<0>(),
            [this, client_socket = t.get<1>()](auto& socket)
            {
                internalHandleMessageToPeer(socket, *client_socket);
            });
    }
    return ret;
}

PeerSocketProxy::SocketVector PeerSocketProxy::getStreamSockets()
{
    return streamSockets;
}

void PeerSocketProxy::internalHandleMessageToPeer(
    zmq::socket_t& peerSocket, zmq::socket_t& clientSocket)
{
    clientSocket.send("", 0, ZMQ_SNDMORE);
    const auto self_order_buffer = boost::endian::native_to_big(selfOrder);
    clientSocket.send(&self_order_buffer, sizeof(OrderParameter), ZMQ_SNDMORE);
    Messaging::forwardMessage(peerSocket, clientSocket);
}

void PeerSocketProxy::internalHandleMessageFromPeer(zmq::socket_t& socket)
{
    if (&socket == peerServerSocket.get()) {
        auto router_id_frame = std::optional<zmq::message_t> {};
        auto msg = zmq::message_t {};
        for (socket.recv(&msg); msg.size() != 0u; socket.recv(&msg)) {
            router_id_frame = std::move(msg);
        }
        // discard the empty message that caused exit from the loop, receive
        // first payload frame
        socket.recv(&msg);
        auto identity = Messaging::identityFromMessage(
            msg, getPtr(router_id_frame));
        if (msg.more()) {
            if (msg.size() != sizeof(OrderParameter)) {
                Messaging::discardMessage(socket);
                return;
            }
            auto peer_order = OrderParameter {};
            std::memcpy(&peer_order, msg.data(), sizeof(OrderParameter));
            boost::endian::big_to_native_inplace(peer_order);
            if (peer_order == selfOrder ||
                peer_order > frontStreamSockets.size() ||
                !authorizer(identity, peer_order)) {
                Messaging::discardMessage(socket);
                return;
            }
            // There is no external peer with the same order as self, so the
            // stream socket corresponding to self does not exist in the stream
            // socket vector. Adjust peer_order accordingly around self order.
            if (peer_order > selfOrder) {
                --peer_order;
            }
            assert(frontStreamSockets[peer_order]);
            Messaging::forwardMessage(socket, *frontStreamSockets[peer_order]);
        }
    }
}

}
}
