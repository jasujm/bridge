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

bool recvMessageHelper(
    std::string& identity, StringVector& message, zmq::socket_t& socket)
{
    auto more = false;
    std::tie(identity, more) = recvMessage(socket);
    if (!more) {
        return false;
    }
    message.clear();
    recvAll(std::back_inserter(message), socket);
    return !message.empty();
}

void sendReplyHelper(
    const std::string& identity, const bool success,
    const StringVector& output, zmq::socket_t& socket)
{
    sendMessage(socket, identity, true);
    sendMessage(socket, std::string {}, true);
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
            sendReplyHelper(identity, true, StringVector {}, socket);
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
