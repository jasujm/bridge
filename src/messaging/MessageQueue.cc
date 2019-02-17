#include "messaging/MessageQueue.hh"

#include "messaging/Identity.hh"
#include "messaging/MessageUtility.hh"
#include "Utility.hh"
#include "Blob.hh"

#include <boost/endian/conversion.hpp>
#include <zmq.hpp>

#include <iterator>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

using MessageVector = std::vector<zmq::message_t>;

class BasicResponse : public Response {
public:
    void handleSetStatus(StatusCode status) override;
    void handleAddFrame(ByteSpan frame) override;

    StatusCode status {REPLY_FAILURE};
    MessageVector frames {};
};

void BasicResponse::handleSetStatus(StatusCode status)
{
    this->status = status;
}

void BasicResponse::handleAddFrame(ByteSpan frame)
{
    this->frames.emplace_back(frame.data(), frame.size());
}

}

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
    auto response = BasicResponse {};
    if (command_handler_entry != handlers.end()) {
        auto& handler = dereference(command_handler_entry->second);
        auto execution = SynchronousExecutionPolicy {};
        handler.handle(
            execution, identityFromMessage(*first_payload_frame, identity_msg),
            first_payload_frame+1, input_frames.end(), response);
    }

    // Echo back everything before first payload, i.e. routing info
    sendMultipart(
        socket, input_frames.begin(), first_payload_frame, ZMQ_SNDMORE);

    // Send status
    socket.send(
        messageFromValue(boost::endian::native_to_big(response.status)),
        ZMQ_SNDMORE);

    // Send command
    socket.send(
        *first_payload_frame, response.frames.empty() ? 0 : ZMQ_SNDMORE);

    // Send all output
    if (!response.frames.empty()) {
        sendMultipart(socket, response.frames.begin(), response.frames.end());
    }
}

}
}
