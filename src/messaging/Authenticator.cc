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
const auto ZAP_ENDPOINT = "inproc://zeromq.zap.01"sv;
const auto CONTROL_ENDPOINT = "inproc://bridge.authenticator.control"sv;
const auto ZAP_VERSION = "1.0"sv;
const auto ZAP_SUCCESS = "200"sv;
const auto ZAP_ERROR = "400"sv;
const auto ZAP_STATUS = "OK"sv;
const auto ZAP_STATUS_ERROR = "Error"sv;
const auto ZAP_EXPECTED_MESSAGE_SIZE = 7;
const auto CURVE_MECHANISM = "CURVE"sv;
const auto ANONYMOUS_USER_ID = "anonymous"sv;

enum class AuthenticatorCommand {
    SYNC,
    ADD_NODE,
};

void zapServer(
    zmq::context_t& context, zmq::socket_t terminationSubscriber,
    Authenticator::NodeMap knownNodes)
{
    auto control_socket = zmq::socket_t {context, zmq::socket_type::rep};
    control_socket.bind(CONTROL_ENDPOINT.data());
    auto zap_socket = zmq::socket_t {context, zmq::socket_type::rep};
    zap_socket.bind(ZAP_ENDPOINT.data());
    auto pollitems = std::array {
        zmq::pollitem_t {
            static_cast<void*>(terminationSubscriber), 0, ZMQ_POLLIN, 0 },
        zmq::pollitem_t {
            static_cast<void*>(control_socket), 0, ZMQ_POLLIN, 0 },
        zmq::pollitem_t { static_cast<void*>(zap_socket), 0, ZMQ_POLLIN, 0 },
    };
    while (true) {
        static_cast<void>(zmq::poll(pollitems.data(), pollitems.size()));
        if (pollitems[0].revents & ZMQ_POLLIN) {
            break;
        }
        if (pollitems[1].revents & ZMQ_POLLIN) {
            auto command = AuthenticatorCommand {};
            control_socket.recv(&command, sizeof(command));
            if (command == AuthenticatorCommand::ADD_NODE) {
                auto key_msg = zmq::message_t {};
                control_socket.recv(&key_msg);
                const auto key_view = messageView(key_msg);
                auto user_id_msg = zmq::message_t {};
                control_socket.recv(&user_id_msg);
                knownNodes.insert_or_assign(
                    Blob(key_view.begin(), key_view.end()),
                    UserId(user_id_msg.data<char>(), user_id_msg.size()));
            }
            assert(!control_socket.getsockopt<int>(ZMQ_RCVMORE));
            control_socket.send("", 0);
        }
        if (pollitems[2].revents & ZMQ_POLLIN) {
            auto input_frames =
                std::array<zmq::message_t, ZAP_EXPECTED_MESSAGE_SIZE> {};
            auto status_code_message = zmq::message_t {};
            auto status_text_message = zmq::message_t {};
            auto user_id_message = zmq::message_t {};

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
                    const auto& user_id_str = user_id_iter->second;
                    user_id_message.rebuild(
                        user_id_str.data(), user_id_str.size());
                } else {
                    user_id_message.rebuild(
                        ANONYMOUS_USER_ID.data(), ANONYMOUS_USER_ID.size());
                }
            }

            zap_socket.send(
                ZAP_VERSION.data(), ZAP_VERSION.size(), ZMQ_SNDMORE);
            zap_socket.send(input_frames[1], ZMQ_SNDMORE); // request id
            zap_socket.send(status_code_message, ZMQ_SNDMORE);
            zap_socket.send(status_text_message, ZMQ_SNDMORE);
            zap_socket.send(user_id_message, ZMQ_SNDMORE);
            zap_socket.send("", 0);
        }
    }
}

void recvReplyFromZapThread(zmq::socket_t& socket)
{
    zmq::message_t reply_msg;
    socket.recv(&reply_msg);
    assert(!reply_msg.more());
}

}

Authenticator::Authenticator(
    zmq::context_t& context,
    zmq::socket_t terminationSubscriber,
    NodeMap knownNodes) :
    context {context},
    controlSocket {context, zmq::socket_type::req},
    worker {
        zapServer, std::ref(context), std::move(terminationSubscriber),
        std::move(knownNodes)}
{
    controlSocket.connect(CONTROL_ENDPOINT.data());
}

void Authenticator::ensureRunning()
{
    const auto cmd_buffer = AuthenticatorCommand::SYNC;
    controlSocket.send(&cmd_buffer, sizeof(cmd_buffer));
    recvReplyFromZapThread(controlSocket);
}

void Authenticator::addNode(const ByteSpan key, const UserIdView userId)
{
    const auto cmd_buffer = AuthenticatorCommand::ADD_NODE;
    controlSocket.send(&cmd_buffer, sizeof(cmd_buffer), ZMQ_SNDMORE);
    controlSocket.send(key.data(), key.size(), ZMQ_SNDMORE);
    controlSocket.send(userId.data(), userId.size());
    recvReplyFromZapThread(controlSocket);
}

}
}
