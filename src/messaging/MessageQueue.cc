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

    template<typename MessageIterator>
    BasicResponse(
        MessageIterator firstPrefix, MessageIterator lastPrefix,
        zmq::message_t& commandFrame);

    void sendResponse(zmq::socket_t& socket);

private:
    void handleSetStatus(StatusCode status) override;
    void handleAddFrame(ByteSpan frame) override;

    std::size_t nStatusFrame;
    MessageVector frames;
};

template<typename MessageIterator>
BasicResponse::BasicResponse(
    MessageIterator firstPrefix, MessageIterator lastPrefix,
    zmq::message_t& commandFrame) :
    // the first frame after the prefix is the status frame
    // thus the prefix length is status frame index
    nStatusFrame {
        static_cast<std::size_t>(std::distance(firstPrefix, lastPrefix))},
    frames(nStatusFrame + 2)
{
    auto replyIter = frames.begin();
    while (firstPrefix != lastPrefix) {
        replyIter->move(&(*firstPrefix));
        ++replyIter; ++firstPrefix;
    }
    frames.back().move(&commandFrame);
}

void BasicResponse::sendResponse(zmq::socket_t& socket)
{
    sendMultipart(socket, frames.begin(), frames.end());
}

void BasicResponse::handleSetStatus(StatusCode status)
{
    assert(nStatusFrame < frames.size());
    const auto data = boost::endian::native_to_big(status);
    frames[nStatusFrame].rebuild(&data, sizeof(data));
}

void BasicResponse::handleAddFrame(ByteSpan frame)
{
    frames.emplace_back(frame.data(), frame.size());
}

class DefaultMessageHandler : public MessageHandler {
private:
    void doHandle(
        SynchronousExecutionPolicy& execution, const Identity& identity,
        const ParameterVector& params, Response& response) override;
};

void DefaultMessageHandler::doHandle(
    SynchronousExecutionPolicy&, const Identity&, const ParameterVector&,
    Response& response)
{
    response.setStatus(REPLY_FAILURE);
}

auto DEFAULT_MESSAGE_HANDLER = DefaultMessageHandler {};

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
    auto payload_frame_iter = input_frames.begin();
    const auto type = socket.getsockopt<zmq::socket_type>(ZMQ_TYPE);
    if (type == zmq::socket_type::router) {
        payload_frame_iter = std::find_if(
            payload_frame_iter, input_frames.end(),
            [](const auto& message) { return message.size() == 0u; });
        // If identity frame missing, silently drop it and don't reply
        if (payload_frame_iter == input_frames.begin()) {
            return;
        }
        identity_msg = &payload_frame_iter[-1];
        // If the empty frame is missing, silently drop it and don't reply
        if (payload_frame_iter == input_frames.end()) {
            return;
        }
        ++payload_frame_iter;
    }

    // If command is missing, silently drop the message and don't reply
    if (payload_frame_iter == input_frames.end()) {
        return;
    }

    const auto handler_iter = handlers.find(messageView(*payload_frame_iter));
    auto& handler = (handler_iter != handlers.end()) ?
        dereference(handler_iter->second) : DEFAULT_MESSAGE_HANDLER;
    auto execution = SynchronousExecutionPolicy {};
    const auto identity = identityFromMessage(
        *payload_frame_iter, identity_msg);
    auto response = BasicResponse(
        input_frames.begin(), payload_frame_iter, *payload_frame_iter);
    handler.handle(
        execution, identity, payload_frame_iter+1, input_frames.end(),
        response);

    response.sendResponse(socket);
}

}
}
