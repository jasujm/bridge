#include "messaging/MessageQueue.hh"

#include "messaging/Identity.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "Utility.hh"
#include "Blob.hh"

#include <zmq.hpp>

#include <iterator>
#include <vector>

namespace Bridge {
namespace Messaging {

using MessageVector = std::vector<zmq::message_t>;

MessageQueue::MessageQueue(HandlerMap handlers) :
    handlers {std::move(handlers)}
{
}

MessageQueue::~MessageQueue() = default;

bool MessageQueue::trySetHandler(
    const ByteSpan command, std::shared_ptr<MessageHandler> handler)
{
    const auto iter = handlers.find(command);
    if (iter != handlers.end()) {
        return iter->second == handler;
    }
    handlers.emplace(Blob(command.begin(), command.end()), std::move(handler));
    return true;
}

void MessageQueue::operator()(zmq::socket_t& socket)
{
    auto input_frames = MessageVector {};
    for(;;) {
        auto& current_frame = input_frames.emplace_back();
        socket.recv(&current_frame);
        if (!current_frame.more()) {
            break;
        }
    }

    auto identity_view = ByteSpan {};
    auto first_payload_frame = input_frames.begin();
    const auto type = socket.getsockopt<zmq::socket_type>(ZMQ_TYPE);
    if (type == zmq::socket_type::router) {
        first_payload_frame = std::find_if(
            first_payload_frame, input_frames.end(),
            [](const auto& message) { return message.size() == 0u; });
        // If identity frame missing, silently drop it and don't reply
        if (first_payload_frame == input_frames.begin()) {
            return;
        }
        identity_view = messageView(first_payload_frame[-1]);
        // If the empty frame is missing, silently drop it and don't reply
        if (first_payload_frame == input_frames.end()) {
            return;
        }
        ++first_payload_frame;
    }

    // If command is missing, silently drop the message and don't reply
    if (first_payload_frame == input_frames.end()) {
        return;
    }
    const auto command = messageView(*first_payload_frame);
    const auto command_handler_entry = handlers.find(command);
    auto success = false;
    auto output_frames = MessageVector {};
    if (command_handler_entry != handlers.end()) {
        auto& handler = dereference(command_handler_entry->second);
        success = handler.handle(
            Identity(identity_view.begin(), identity_view.end()),
            first_payload_frame+1, input_frames.end(),
            [&output_frames](const auto& bytes)
            {
                output_frames.emplace_back(bytes.begin(), bytes.end());
            });
    }

    // Echo back everything before first payload, i.e. routing info
    auto send_func = [&socket](auto& message)
    {
        socket.send(message, ZMQ_SNDMORE);
    };
    std::for_each(input_frames.begin(), first_payload_frame, send_func);

    // Send status
    sendValue(socket, success ? REPLY_SUCCESS : REPLY_FAILURE, true);

    // Send command
    socket.send(*first_payload_frame, output_frames.empty() ? 0 : ZMQ_SNDMORE);

    // Send all output
    if (!output_frames.empty()) {
        const auto second_to_last_output_frame = output_frames.end() - 1;
        std::for_each(
            output_frames.begin(), second_to_last_output_frame, send_func);
        socket.send(output_frames.back(), 0);
    }
}

}
}
