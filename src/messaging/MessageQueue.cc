#include "messaging/MessageQueue.hh"

#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "Utility.hh"

namespace Bridge {
namespace Messaging {

MessageQueue::BasicResponse::BasicResponse(
    MessageVector& inputFrames, const std::ptrdiff_t nPrefix) :
    // the first frame after the prefix is the status frame
    // thus the prefix length is status frame index
    nStatusFrame {nPrefix},
    frames(nPrefix + 1)
{
    assert(0 <= nPrefix && nPrefix < ssize(inputFrames));
    for (const auto n : to(nPrefix)) {
        frames[n].move(inputFrames[n]);
    }
}

void MessageQueue::BasicResponse::sendResponse(Socket& socket)
{
    sendMultipart(socket, frames.begin(), frames.end());
}

void MessageQueue::BasicResponse::abortWithFailure(Socket& socket)
{
    const auto iter = frames.begin();
    sendMultipart(socket, iter, iter + nStatusFrame, true);
    sendMessage(socket, messageBuffer(REPLY_FAILURE));
}

void MessageQueue::BasicResponse::handleSetStatus(ByteSpan status)
{
    assert(0 <= nStatusFrame && nStatusFrame < ssize(frames));
    frames[nStatusFrame].rebuild(status.data(), status.size());
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

void MessageQueue::operator()(Socket& socket)
{
    auto input_frames = MessageVector {};
    recvMultipart(socket, std::back_inserter(input_frames));

    auto* identity_msg = static_cast<Message*>(nullptr);
    auto payload_frame_iter = input_frames.begin();
    const auto type = socket.getsockopt<SocketType>(ZMQ_TYPE);
    if (type == SocketType::router) {
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

    // If the command and tag are missing, silently drop the message
    // and don't reply
    if (input_frames.end() - payload_frame_iter < 2) {
        return;
    }

    const auto executor_iter = executors.find(
        messageView(payload_frame_iter[1]));
    auto& executor = (executor_iter != executors.end()) ?
        executor_iter->second : defaultExecutor;
    auto identity = identityFromMessage(*payload_frame_iter, identity_msg);
    const auto n_prefix = payload_frame_iter - input_frames.begin() + 1;
    executor(std::move(identity), std::move(input_frames), n_prefix, socket);
}

}
}
