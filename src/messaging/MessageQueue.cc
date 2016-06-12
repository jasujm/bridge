#include "messaging/MessageQueue.hh"

#include "messaging/MessageHandler.hh"
#include "messaging/MessageUtility.hh"
#include "messaging/Replies.hh"
#include "Utility.hh"

#include <cassert>
#include <iterator>
#include <vector>

namespace Bridge {
namespace Messaging {

namespace {

using StringVector = std::vector<std::string>;

bool recvIdentityIfRouter(std::string& identity, zmq::socket_t& socket)
{
    const auto type = socket.getsockopt<zmq::socket_type>(ZMQ_TYPE);
    bool more = true;
    if (type == zmq::socket_type::router) {
        std::tie(identity, more) = recvMessage(socket);
    }
    return more;
}

bool recvMessageHelper(
    std::string& identity, StringVector& message, zmq::socket_t& socket)
{
    if (!recvIdentityIfRouter(identity, socket)) {
        return false;
    }
    recvAll(std::back_inserter(message), socket);
    return !message.empty();
}

bool sendIdentityIfRouter(const std::string& identity, zmq::socket_t& socket)
{
    const auto type = socket.getsockopt<zmq::socket_type>(ZMQ_TYPE);
    if (type == zmq::socket_type::router) {
        sendMessage(socket, identity, true);
    }
}

void sendReplyHelper(
    const std::string& identity, const bool success,
    const StringVector& output, zmq::socket_t& socket)
{
    sendIdentityIfRouter(identity, socket);
    sendEmptyFrameIfNecessary(socket);
    sendMessage(
        socket,
        success ? REPLY_SUCCESS : REPLY_FAILURE,
        !output.empty());
    sendMessage(socket, output.begin(), output.end());
}

}

MessageQueue::MessageQueue(
    MessageQueue::HandlerMap handlers,
    boost::optional<std::string> terminateCommand) :
    handlers {std::move(handlers)},
    terminateCommand {std::move(terminateCommand)}
{
}

MessageQueue::~MessageQueue() = default;

bool MessageQueue::operator()(zmq::socket_t& socket)
{
    auto output = std::vector<std::string> {};
    auto message = std::vector<std::string> {};
    auto identity = std::string {};
    if (recvMessageHelper(identity, message, socket)) {
        assert(!message.empty());
        const auto command = message.front();
        if (command == terminateCommand) {
            return false;
        }
        const auto entry = handlers.find(command);
        if (entry != handlers.end()) {
            auto& handler = dereference(entry->second);
            assert(output.empty());
            output.push_back(command);
            const auto success = handler.handle(
                identity, std::next(message.begin()), message.end(),
                std::back_inserter(output));
            sendReplyHelper(identity, success, output, socket);
            return true;
        }
    }
    sendReplyHelper(identity, false, output, socket);
    return true;
}

}
}
