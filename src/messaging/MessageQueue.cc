#include "messaging/MessageQueue.hh"

#include "messaging/MessageHandler.hh"
#include "messaging/MessageUtility.hh"
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
    // There should be at least two more frames, the first one which is empty,
    // and the second one which contains the command
    return message.size() >= 2 && message.front().size() == 0;
}

void sendReplyHelper(
    const std::string& identity, const bool success,
    const StringVector& output, zmq::socket_t& socket)
{
    sendMessage(socket, identity, true);
    sendMessage(socket, std::string {}, true);
    sendMessage(
        socket,
        success ? MessageQueue::REPLY_SUCCESS : MessageQueue::REPLY_FAILURE,
        !output.empty());
    sendMessage(socket, output.begin(), output.end());
}

}

const std::string MessageQueue::REPLY_SUCCESS {"success"};
const std::string MessageQueue::REPLY_FAILURE {"failure"};

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
        assert(message.size() >= 2);
        const auto command = message.at(1);
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
                identity, std::next(message.begin(), 2), message.end(),
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
