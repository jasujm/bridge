#include "main/PeerCommandSender.hh"

#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Bridge {
namespace Main {

using namespace Messaging;

PeerCommandSender::Peer::Peer(
    zmq::context_t& context, const std::string& endpoint) :
    socket {std::make_shared<zmq::socket_t>(context, zmq::socket_type::dealer)},
    success {false}
{
    socket->connect(endpoint);
}

std::shared_ptr<zmq::socket_t> PeerCommandSender::addPeer(
    zmq::context_t& context, const std::string& endpoint)
{
    peers.emplace_back(context, endpoint);
    return peers.back().socket;
}

void PeerCommandSender::processReply(zmq::socket_t& socket)
{
    auto iter = std::find_if(
        peers.begin(), peers.end(),
        [&socket](const auto& peer) { return peer.socket.get() == &socket; });
    if (iter == peers.end()) {
        throw std::invalid_argument("Socket is not peer socket");
    }
    auto& success = iter->success;

    auto message = Message {};
    recvAll(std::back_inserter(message), socket);
    if (messages.empty()) {
        return;
    }
    const auto& current_message = messages.front();
    assert(!current_message.empty());
    if (
        isSuccessfulReply(
            message.begin(), message.end(), current_message.front())) {
        success = true;
        if (
            std::all_of(
                peers.begin(), peers.end(),
                [](const auto& peer) { return peer.success; })) {
            messages.pop();
            if (!messages.empty()) {
                internalSendMessageToAll();
            }
        }
    } else {
        internalSendMessage(socket);
    }
}

void PeerCommandSender::internalSendMessage(zmq::socket_t& socket)
{
    assert(!messages.empty());
    const auto& message = messages.front();
    sendEmptyFrameIfNecessary(socket);
    sendMessage(socket, message.begin(), message.end());
}

void PeerCommandSender::internalSendMessageToAll()
{
    for (auto& peer : peers) {
        assert(peer.socket);
        internalSendMessage(*peer.socket);
        peer.success = false;
    }
}

void PeerCommandSender::internalAddMessage(Message message)
{
    messages.emplace(std::move(message));
    if (messages.size() == 1u) {
        internalSendMessageToAll();
    }
}

}
}
