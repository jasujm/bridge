#include "main/CallbackScheduler.hh"
#include "main/PeerCommandSender.hh"

#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "Blob.hh"
#include "Enumerate.hh"

namespace Bridge {
namespace Main {

using namespace Messaging;

using namespace std::chrono_literals;

namespace {

const auto INITIAL_RESEND_TIMEOUT = 50ms;
const auto MAX_RESEND_TIMEOUT =
    std::chrono::duration_cast<std::chrono::milliseconds>(1min);

}

PeerCommandSender::Peer::Peer(
    zmq::context_t& context, const std::string& endpoint,
    const CurveKeys* const keys, const ByteSpan serverKey) :
    socket {std::make_shared<zmq::socket_t>(context, zmq::socket_type::dealer)},
    resendTimeout {INITIAL_RESEND_TIMEOUT},
    success {false}
{
    setupCurveClient(*socket, keys, serverKey);
    socket->connect(endpoint);
}

std::shared_ptr<zmq::socket_t> PeerCommandSender::addPeer(
    zmq::context_t& context, const std::string& endpoint,
    const CurveKeys* const keys, const ByteSpan serverKey)
{
    peers.emplace_back(context, endpoint, keys, serverKey);
    return peers.back().socket;
}

PeerCommandSender::PeerCommandSender(
    std::shared_ptr<CallbackScheduler> callbackScheduler) :
    callbackScheduler {std::move(callbackScheduler)},
    messages {},
    peers {}
{
}

void PeerCommandSender::processReply(zmq::socket_t& socket)
{
    auto iter = std::find_if(
        peers.begin(), peers.end(),
        [&socket](const auto& peer) { return peer.socket.get() == &socket; });
    if (iter == peers.end()) {
        throw std::invalid_argument("Socket is not peer socket");
    }
    auto message = std::vector<zmq::message_t> {};
    recvEmptyFrameIfNecessary(socket);
    recvMultipart(socket, std::back_inserter(message));
    if (messages.empty()) {
        return;
    }
    const auto& current_message = messages.front();
    assert(!current_message.empty());
    const auto reply_iter = isSuccessfulReply(message.begin(), message.end());
    if (reply_iter != message.end() && *reply_iter == current_message.front()) {
        iter->success = true;
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
        dereference(callbackScheduler).callOnce(
            [this, &socket](){
                internalSendMessage(socket);
            }, iter->resendTimeout);
        iter->resendTimeout = std::min(
            2 * iter->resendTimeout, MAX_RESEND_TIMEOUT);
    }
}

void PeerCommandSender::internalSendMessage(zmq::socket_t& socket)
{
    assert(!messages.empty());
    auto& message = messages.front();
    sendEmptyFrameIfNecessary(socket);
    for (auto&& e : enumerate(message)) {
        auto msg = zmq::message_t {};
        msg.copy(&e.second);
        socket.send(
            msg, static_cast<std::size_t>(e.first) + 1u < message.size() ?
                ZMQ_SNDMORE : 0);
    }
}

void PeerCommandSender::internalSendMessageToAll()
{
    for (auto& peer : peers) {
        assert(peer.socket);
        internalSendMessage(*peer.socket);
        peer.resendTimeout = INITIAL_RESEND_TIMEOUT;
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
