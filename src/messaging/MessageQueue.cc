#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"

#include <cassert>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

auto handleMessage(
    zmq::socket_t& socket, const MessageQueue::Handler& handler, bool more)
{
    auto parameters = std::vector<std::string> {};
    while (more) {
        const auto message = recvMessage(socket);
        parameters.push_back(message.first);
        more = message.second;
    }
    return handler(parameters);
}

void sendReply(
    zmq::socket_t& socket, const std::string& reply,
    const MessageQueue::HandlerParameters& parameters)
{
    auto param_begin = parameters.begin();
    auto more = (param_begin != parameters.end());
    sendMessage(socket, reply, more);
    while (more) {
        const auto param_next = std::next(param_begin);
        more = (param_next != parameters.end());
        sendMessage(socket, *param_begin, more);
        param_begin = param_next;
    }
}

}

const std::string MessageQueue::MESSAGE_TERMINATE {"terminate"};
const std::string MessageQueue::MESSAGE_ERROR {"error"};

class MessageQueue::Impl {
public:
    Impl(HandlerMap handlers, zmq::socket_t socket);

    void run();

private:
    HandlerMap handlers;
    zmq::socket_t socket;
    bool go {true};
};

MessageQueue::Impl::Impl(HandlerMap handlers, zmq::socket_t socket) :
    handlers {std::move(handlers)},
    socket {std::move(socket)}
{
    this->handlers.emplace(
        MESSAGE_TERMINATE,
        [this](const auto&)
        {
            go = false;
            const auto empty_parameters = std::vector<std::string> {};
            return std::make_pair(std::string {}, empty_parameters);
        });
}

void MessageQueue::Impl::run()
{
    while (go) { // go is set to false in handler for MESSAGE_TERMINATE
        const auto message = recvMessage(socket);
        const auto handler = handlers.find(message.first);
        if (handler != handlers.end()) {
            const auto reply = handleMessage(
                socket, handler->second, message.second);
            sendReply(socket, reply.first, reply.second);
        } else {
            sendMessage(socket, MESSAGE_ERROR);
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
    MessageQueue::HandlerMap handlers, zmq::socket_t socket) :
    impl {std::make_unique<Impl>(std::move(handlers), std::move(socket))}
{
}

}
}
