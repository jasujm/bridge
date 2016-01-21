#include "messaging/MessageQueue.hh"
#include "messaging/MessageUtility.hh"

#include <cassert>

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
        [this]()
        {
            go = false;
            return std::string {};
        });
}

void MessageQueue::Impl::run()
{
    while (go) {
        const auto command = recvMessage(socket);
        const auto handler = handlers.find(command);
        if (handler != handlers.end()) {
            const auto reply = handler->second();
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
