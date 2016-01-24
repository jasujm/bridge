#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"

#include <cassert>
#include <vector>

namespace Bridge {
namespace Messaging {

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
            return std::string {};
        });
}

void MessageQueue::Impl::run()
{
    while (go) {
        auto message = recvMessage(socket);
        const auto handler = handlers.find(message.first);
        if (handler != handlers.end()) {
            auto parameters = std::vector<std::string> {};
            auto more = message.second;
            while (more) {
                message = recvMessage(socket);
                parameters.push_back(message.first);
                more = message.second;
            }
            const auto reply = handler->second(parameters);
            sendMessage(socket, reply);
        }
        else
        {
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
