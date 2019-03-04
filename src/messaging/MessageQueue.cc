#include "messaging/MessageQueue.hh"

#include "messaging/MessageUtility.hh"

#include <boost/endian/conversion.hpp>
#include <zmq.hpp>

namespace Bridge {
namespace Messaging {

MessageQueue::BasicResponse::BasicResponse(
    MessageVector& inputFrames, const std::size_t nPrefix) :
    // the first frame after the prefix is the status frame
    // thus the prefix length is status frame index
    nStatusFrame {nPrefix},
    frames(nPrefix + 2)
{
    assert(nPrefix < inputFrames.size());
    for (const auto n : to(nPrefix)) {
        frames[n].move(&inputFrames[n]);
    }
    frames.back().move(&inputFrames[nPrefix]);
}

void MessageQueue::BasicResponse::sendResponse(zmq::socket_t& socket)
{
    sendMultipart(socket, frames.begin(), frames.end());
}

void MessageQueue::BasicResponse::handleSetStatus(StatusCode status)
{
    assert(nStatusFrame < frames.size());
    const auto data = boost::endian::native_to_big(status);
    frames[nStatusFrame].rebuild(&data, sizeof(data));
}

void MessageQueue::BasicResponse::handleAddFrame(ByteSpan frame)
{
    frames.emplace_back(frame.data(), frame.size());
}

namespace {

class DefaultMessageHandler : public MessageHandler {
private:
    void doHandle(
        ExecutionContext, const Identity& identity,
        const ParameterVector& params, Response& response) override;
};

void DefaultMessageHandler::doHandle(
    ExecutionContext, const Identity&, const ParameterVector&,
    Response& response)
{
    response.setStatus(REPLY_FAILURE);
}

}

MessageQueue::MessageQueue() :
    defaultExecutor {
        internalCreateExecutor<SynchronousExecutionPolicy>(
            std::make_shared<DefaultMessageHandler>())}
{
}

MessageQueue::MessageQueue(
    std::initializer_list<
        std::pair<ByteSpan, std::shared_ptr<MessageHandler>>> handlers) :
    MessageQueue {}
{
    for (auto&& [command, handler] : handlers) {
        trySetHandler(command, std::move(handler));
    }
}

MessageQueue::~MessageQueue() = default;

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

    const auto executor_iter = executors.find(messageView(*payload_frame_iter));
    auto& executor = (executor_iter != executors.end()) ?
        executor_iter->second : defaultExecutor;
    auto identity = identityFromMessage(*payload_frame_iter, identity_msg);
    const auto n_prefix = static_cast<std::size_t>(payload_frame_iter - input_frames.begin());
    executor(
        std::move(identity), std::move(input_frames), n_prefix, socket);
}

}
}
