#include "main/PeerCommandSender.hh"

#include "messaging/CallbackScheduler.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "Blob.hh"
#include "Enumerate.hh"
#include "Utility.hh"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Bridge {
namespace Main {

using namespace Messaging;

using namespace std::chrono_literals;

namespace {

constexpr auto INITIAL_RESEND_TIMEOUT = 50ms;
constexpr auto MAX_RESEND_TIMEOUT =
    std::chrono::duration_cast<std::chrono::milliseconds>(1min);

}

PeerCommandSender::Peer::Peer(
    Messaging::MessageContext& context, const std::string_view endpoint,
    const CurveKeys* const keys, const ByteSpan serverKey) :
    socket {makeSharedSocket(context, Messaging::SocketType::dealer)},
    resendTimeout {INITIAL_RESEND_TIMEOUT},
    success {false}
{
    setupCurveClient(*socket, keys, serverKey);
    connectSocket(*socket, endpoint);
}

Messaging::SharedSocket PeerCommandSender::addPeer(
    Messaging::MessageContext& context, const std::string_view endpoint,
    const CurveKeys* const keys, const ByteSpan serverKey)
{
    peers.emplace_back(context, endpoint, keys, serverKey);
    return peers.back().socket;
}

PeerCommandSender::PeerCommandSender(
    std::shared_ptr<Messaging::CallbackScheduler> callbackScheduler) :
    callbackScheduler {std::move(callbackScheduler)},
    messages {},
    peers {}
{
}

void PeerCommandSender::operator()(Messaging::Socket& socket)
{
    auto iter = std::find_if(
        peers.begin(), peers.end(),
        [&socket](const auto& peer) { return peer.socket.get() == &socket; });
    if (iter == peers.end()) {
        throw std::invalid_argument("Socket is not peer socket");
    }
    auto message = std::vector<Messaging::Message> {};
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
        dereference(callbackScheduler).callLater(
            iter->resendTimeout,
            &PeerCommandSender::internalSendMessage,
            this, std::ref(socket));
        iter->resendTimeout = std::min(
            2 * iter->resendTimeout, MAX_RESEND_TIMEOUT);
    }
}

void PeerCommandSender::internalSendMessage(Messaging::Socket& socket)
{
    assert(!messages.empty());
    auto& message = messages.front();
    sendEmptyFrameIfNecessary(socket);
    for (auto&& e : enumerate(message)) {
        auto msg = Messaging::Message {};
        msg.copy(e.second);
        sendMessage(socket, std::move(msg), e.first + 1 < ssize(message));
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
