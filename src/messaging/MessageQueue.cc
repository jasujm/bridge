#include "messaging/MessageQueue.hh"

#include "messaging/Identity.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "Utility.hh"
#include "Blob.hh"

#include <boost/endian/conversion.hpp>
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
    recvMultipart(socket, std::back_inserter(input_frames));

    auto* identity_msg = static_cast<zmq::message_t*>(nullptr);
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
        identity_msg = &first_payload_frame[-1];
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
            identityFromMessage(*first_payload_frame, identity_msg),
            first_payload_frame+1, input_frames.end(),
            [&output_frames](const auto& bytes)
            {
                output_frames.emplace_back(bytes.begin(), bytes.end());
            });
    }

    // Echo back everything before first payload, i.e. routing info
    sendMultipart(
        socket, input_frames.begin(), first_payload_frame, ZMQ_SNDMORE);

    // Send status
    const auto status = success ? REPLY_SUCCESS : REPLY_FAILURE;
    socket.send(
        messageFromValue(boost::endian::native_to_big(status)), ZMQ_SNDMORE);

    // Send command
    socket.send(*first_payload_frame, output_frames.empty() ? 0 : ZMQ_SNDMORE);

    // Send all output
    if (!output_frames.empty()) {
        sendMultipart(socket, output_frames.begin(), output_frames.end());
    }
}

}
}
