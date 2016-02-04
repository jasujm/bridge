#include "messaging/MessageHandler.hh"
#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"

#include <cassert>
#include <functional>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

auto handleMessage(
    zmq::socket_t& socket, MessageHandler& handler, bool more)
{
    auto parameters = std::vector<std::string> {};
    while (more) {
        const auto message = recvMessage(socket);
        parameters.push_back(message.first);
        more = message.second;
    }
    return handler.handle(parameters.begin(), parameters.end());
}

void sendReply(
    zmq::socket_t& socket, const std::string& reply,
    const MessageHandler::ReturnValue& params)
{
    auto param_begin = params.begin();
    auto more = (param_begin != params.end());
    sendMessage(socket, reply, more);
    while (more) {
        const auto param_next = std::next(param_begin);
        more = (param_next != params.end());
        sendMessage(socket, *param_begin, more);
        param_begin = param_next;
    }
}

class TerminateMessageHandler : public MessageHandler {
public:
    TerminateMessageHandler(bool& go) : go {go} {}
    ReturnValue doHandle(ParameterRange) override
    {
        go = false;
        return {};
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
        const std::string& address);

    void run();

private:

    HandlerMap handlers;
    zmq::socket_t socket;
    bool go {true};
};

MessageQueue::Impl::Impl(
    HandlerMap handlers, zmq::context_t& context, const std::string& address) :
    handlers {std::move(handlers)},
    socket {context, zmq::socket_type::rep}
{
    this->handlers.emplace(
        MESSAGE_TERMINATE,
        std::make_shared<TerminateMessageHandler>(std::ref(go)));
    socket.bind(address);
}

void MessageQueue::Impl::run()
{
    while (go) { // go is set to false in handler for MESSAGE_TERMINATE
        const auto message = recvMessage(socket);
        const auto handler = handlers.find(message.first);
        if (handler != handlers.end()) {
            assert(handler->second);
            const auto reply = handleMessage(
                socket, *handler->second, message.second);
            sendReply(socket, REPLY_SUCCESS, reply);
        } else {
            sendMessage(socket, REPLY_FAILURE);
        }
    }
}

MessageQueue::~MessageQueue() = default;

void MessageQueue::run()
{
    assert(impl);
    impl->run();
}

MessageQueue::MessageQueue(
    MessageQueue::HandlerMap handlers, zmq::context_t& context,
    const std::string& address) :
    impl {std::make_unique<Impl>(std::move(handlers), context, address)}
{
}

}
}
