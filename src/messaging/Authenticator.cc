#include "messaging/Authenticator.hh"

#include "messaging/MessageUtility.hh"

#include <array>
#include <cassert>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

using namespace std::string_view_literals;
using namespace Bridge::BlobLiterals;

constexpr auto ZAP_ENDPOINT = "inproc://zeromq.zap.01"sv;
constexpr auto CONTROL_ENDPOINT = "inproc://bridge.authenticator.control"sv;
constexpr auto ZAP_VERSION = "1.0"_BS;
constexpr auto ZAP_SUCCESS = "200"_BS;
constexpr auto ZAP_ERROR = "400"_BS;
constexpr auto ZAP_STATUS = "OK"_BS;
constexpr auto ZAP_STATUS_ERROR = "Error"_BS;
constexpr auto ZAP_EXPECTED_MESSAGE_SIZE = 7;
constexpr auto CURVE_MECHANISM = "CURVE"_BS;
constexpr auto ANONYMOUS_USER_ID = "anonymous"sv;

enum class AuthenticatorCommand {
    SYNC,
    ADD_NODE,
};

void zapServer(
    MessageContext& context, Socket terminationSubscriber,
    Authenticator::NodeMap knownNodes)
{
    auto control_socket = Socket {context, SocketType::rep};
    bindSocket(control_socket, CONTROL_ENDPOINT);
    auto zap_socket = Socket {context, SocketType::rep};
    bindSocket(zap_socket, ZAP_ENDPOINT);
    auto pollitems = std::array {
        Pollitem { terminationSubscriber.handle(), 0, ZMQ_POLLIN, 0 },
        Pollitem { control_socket.handle(), 0, ZMQ_POLLIN, 0 },
        Pollitem { zap_socket.handle(), 0, ZMQ_POLLIN, 0 },
    };
    while (true) {
        pollSockets(pollitems);
        if (pollitems[0].revents & ZMQ_POLLIN) {
            break;
        }
        if (pollitems[1].revents & ZMQ_POLLIN) {
            auto command = AuthenticatorCommand {};
            recvMessage(
                control_socket, messageBuffer(&command, sizeof(command)));
            if (command == AuthenticatorCommand::ADD_NODE) {
                auto key_msg = Message {};
                recvMessage(control_socket, key_msg);
                const auto key_view = messageView(key_msg);
                auto user_id_msg = Message {};
                recvMessage(control_socket, user_id_msg);
                knownNodes.insert_or_assign(
                    Blob(key_view.begin(), key_view.end()),
                    UserId(user_id_msg.data<char>(), user_id_msg.size()));
            }
            assert(!control_socket.getsockopt<int>(ZMQ_RCVMORE));
            sendEmptyMessage(control_socket);
        }
        if (pollitems[2].revents & ZMQ_POLLIN) {
            auto input_frames =
                std::array<Message, ZAP_EXPECTED_MESSAGE_SIZE> {};
            auto status_code_message = Message {};
            auto status_text_message = Message {};
            auto user_id_view = UserIdView {};

            const auto received_parts_info = recvMultipart(
                zap_socket, input_frames.begin(), input_frames.size());

            if (received_parts_info.second != ZAP_EXPECTED_MESSAGE_SIZE ||
                messageView(input_frames[0]) != asBytes(ZAP_VERSION) ||
                messageView(input_frames[5]) != asBytes(CURVE_MECHANISM)) {
                status_code_message.rebuild(ZAP_ERROR.data(), ZAP_ERROR.size());
                status_text_message.rebuild(
                    ZAP_STATUS_ERROR.data(), ZAP_STATUS_ERROR.size());
            } else {
                status_code_message.rebuild(
                    ZAP_SUCCESS.data(), ZAP_SUCCESS.size());
                status_text_message.rebuild(
                    ZAP_STATUS.data(), ZAP_STATUS.size());

                auto& public_key_message = input_frames[6];
                const auto user_id_iter =
                    knownNodes.find(messageView(public_key_message));
                if (user_id_iter != knownNodes.end()) {
                    user_id_view = user_id_iter->second;
                } else {
                    user_id_view = ANONYMOUS_USER_ID;
                }
            }

            sendMessage(zap_socket, messageBuffer(ZAP_VERSION), true);
            sendMessage(zap_socket, std::move(input_frames[1]), true);
            sendMessage(zap_socket, std::move(status_code_message), true);
            sendMessage(zap_socket, std::move(status_text_message), true);
            sendMessage(zap_socket, messageBuffer(user_id_view), true);
            sendEmptyMessage(zap_socket);
        }
    }
}

}

Authenticator::Authenticator(
    MessageContext& context, Socket terminationSubscriber, NodeMap knownNodes) :
    context {context},
    controlSocket {context, SocketType::req},
    worker {
        zapServer, std::ref(context), std::move(terminationSubscriber),
        std::move(knownNodes)}
{
    connectSocket(controlSocket, CONTROL_ENDPOINT);
}

void Authenticator::ensureRunning()
{
    const auto cmd_buffer = AuthenticatorCommand::SYNC;
    sendMessage(controlSocket, messageBuffer(&cmd_buffer, sizeof(cmd_buffer)));
    discardMessage(controlSocket);
}

void Authenticator::addNode(const ByteSpan key, const UserIdView userId)
{
    const auto cmd_buffer = AuthenticatorCommand::ADD_NODE;
    sendMessage(controlSocket, messageBuffer(&cmd_buffer, sizeof(cmd_buffer)), true);
    sendMessage(controlSocket, messageBuffer(key), true);
    sendMessage(controlSocket, messageBuffer(userId));
    discardMessage(controlSocket);
}

}
}
