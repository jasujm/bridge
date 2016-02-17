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
    TerminateMessageHandler(bool& go) : go {go} {}
    bool doHandle(ParameterRange) override
    {
        go = false;
        return true;
    }
private:
    bool& go;
};

}

const std::string MessageQueue::MESSAGE_TERMINATE {"terminate"};
const std::string MessageQueue::REPLY_SUCCESS {"success"};
const std::string MessageQueue::REPLY_FAILURE {"failure"};

class MessageQueue::Impl {
public:
    Impl(
        HandlerMap handlers, zmq::context_t& context,
        const std::string& endpoint);

    void run();

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
    this->handlers.emplace(
        MESSAGE_TERMINATE,
        std::make_shared<TerminateMessageHandler>(go));
    socket.bind(endpoint);
}

void MessageQueue::Impl::run()
{
    while (go) { // go is set to false in handler for MESSAGE_TERMINATE
        auto message = std::vector<std::string> {};
        recvAll(std::back_inserter(message), socket);
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

MessageQueue::MessageQueue(
    MessageQueue::HandlerMap handlers, zmq::context_t& context,
    const std::string& endpoint) :
    impl {std::make_unique<Impl>(std::move(handlers), context, endpoint)}
{
}

MessageQueue::~MessageQueue() = default;

void MessageQueue::run()
{
    assert(impl);
    impl->run();
}

}
}
