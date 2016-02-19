#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"
#include "Utility.hh"

#include <cassert>
#include <iterator>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

class TerminateMessageHandler : public MessageHandler {
public:
    TerminateMessageHandler(MessageQueue& queue) : queue {queue} {}
    bool doHandle(ParameterRange) override
    {
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

    HandlerMap handlers;
    zmq::socket_t socket;
    bool go {true};
};

MessageQueue::Impl::Impl(
    HandlerMap handlers, zmq::context_t& context, const std::string& endpoint) :
    handlers {std::move(handlers)},
    socket {context, zmq::socket_type::rep}
{
    socket.bind(endpoint);
}

void MessageQueue::Impl::run()
{
    while (go) {
        auto message = std::vector<std::string> {};
        try {
            recvAll(std::back_inserter(message), socket);
        } catch (const zmq::error_t& e) {
            // If receiving failed because of interrupt signal, terminate
            if (e.num() == EINTR) {
                terminate();
                break;
            }
            // Otherwise rethrow the exception
            throw;
        }
        const auto entry = handlers.find(message.at(0));
        if (entry != handlers.end()) {
            auto& handler = dereference(entry->second);
            const auto success = handler.handle(
                std::next(message.begin()), message.end());
            sendMessage(socket, success ? REPLY_SUCCESS : REPLY_FAILURE);
        } else {
            sendMessage(socket, REPLY_FAILURE);
        }
    }
}

void MessageQueue::Impl::terminate()
{
    go = false;
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
