#include "main/PeerCommandSender.hh"

#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"

#include <algorithm>
#include <utility>

namespace Bridge {
namespace Main {

using namespace Messaging;

void PeerCommandSender::addPeer(zmq::socket_t& peer)
{
    peers.emplace(&peer, false);
}

void PeerCommandSender::processReply(zmq::socket_t& socket)
{
    auto& success = peers.at(&socket);

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
                [](const auto& peer) { return peer.second; })) {
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
        assert(peer.first);
        internalSendMessage(*peer.first);
        peer.second = false;
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
