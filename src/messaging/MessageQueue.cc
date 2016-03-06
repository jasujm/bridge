#include "messaging/MessageQueue.hh"

#include "messaging/MessageHandler.hh"
#include "messaging/MessageUtility.hh"
#include "Utility.hh"

#include <atomic>
#include <cassert>
#include <iterator>
#include <tuple>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

class TerminateMessageHandler : public MessageHandler {
public:
    TerminateMessageHandler(MessageQueue& queue) : queue {queue} {}
    bool doHandle(const std::string&, ParameterRange) override
    {
        // TODO: We should check identity and accept termination only from the
        // instance that controls this application
        queue.terminate();
        return true;
    }

private:
    MessageQueue& queue;
};

MessageQueue::HandlerMap& addTerminateHandler(
    MessageQueue::HandlerMap& handlerMap,
    MessageQueue& queue,
    const boost::optional<std::string>& terminateCommand)
{
    if (terminateCommand) {
        handlerMap.emplace(
            *terminateCommand,
            std::make_shared<TerminateMessageHandler>(queue));
    }
    return handlerMap;
}

bool recvMessageHelper(
    std::string& identity, std::vector<std::string>& message,
    zmq::socket_t& socket)
{
    auto more = false;
    std::tie(identity, more) = recvMessage(socket);
    if (!more) {
        return false;
    }
    message.clear();
    recvAll(std::back_inserter(message), socket);
    // There should be at least two more frames, the first one which is empty,
    // and the second one which contains the command
    return message.size() >= 2 && message.front().size() == 0;
}

void sendReplyHelper(
    const std::string& identity, const bool success, zmq::socket_t& socket)
{
    sendMessage(socket, identity, true);
    sendMessage(socket, std::string {}, true);
    sendMessage(
        socket,
        success ? MessageQueue::REPLY_SUCCESS : MessageQueue::REPLY_FAILURE);
}

}

const std::string MessageQueue::REPLY_SUCCESS {"success"};
const std::string MessageQueue::REPLY_FAILURE {"failure"};

class MessageQueue::Impl {
public:
    Impl(
        HandlerMap handlers, zmq::context_t& context,
        const std::string& endpoint);

    void run();

    void terminate();

private:

    void messageLoop();

    HandlerMap handlers;
    zmq::socket_t socket;
    std::atomic<bool> go {true};
};

MessageQueue::Impl::Impl(
    HandlerMap handlers, zmq::context_t& context, const std::string& endpoint) :
    handlers {std::move(handlers)},
    socket {context, zmq::socket_type::router}
{
    socket.bind(endpoint);
}

void MessageQueue::Impl::run()
{
    try {
        messageLoop();
    } catch (const zmq::error_t& e) {
        // If receiving failed because of interrupt signal, terminate
        if (e.num() == EINTR) {
            terminate();
        }
        // Otherwise rethrow the exception
        else {
            throw;
        }
    }
}

void MessageQueue::Impl::terminate()
{
    go = false;
}

void MessageQueue::Impl::messageLoop()
{
    auto message = std::vector<std::string> {};
    auto identity = std::string {};
    while (go) {
        if (recvMessageHelper(identity, message, socket)) {
            assert(message.size() >= 2);
            const auto entry = handlers.find(message.at(1));
            if (entry != handlers.end()) {
                auto& handler = dereference(entry->second);
                const auto success = handler.handle(
                    identity, std::next(message.begin(), 2), message.end());
                sendReplyHelper(identity, success, socket);
                continue;
            }
        }
        sendReplyHelper(identity, false, socket);
    }
}

MessageQueue::MessageQueue(
    MessageQueue::HandlerMap handlers, zmq::context_t& context,
    const std::string& endpoint,
    const boost::optional<std::string>& terminateCommand) :
    impl {
        std::make_unique<Impl>(
            std::move(addTerminateHandler(handlers, *this, terminateCommand)),
            context, endpoint)}
{
}

MessageQueue::~MessageQueue() = default;

void MessageQueue::run()
{
    assert(impl);
    impl->run();
}

void MessageQueue::terminate()
{
    assert(impl);
    impl->terminate();
}

}
}
