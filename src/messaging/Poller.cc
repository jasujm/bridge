#include "messaging/Poller.hh"

#include <stdexcept>

namespace Bridge {
namespace Messaging {

Poller::~Poller() = default;

void Poller::addPollable(PollableSocket socket, SocketCallback callback)
{
    if (!socket || !callback) {
        throw std::invalid_argument {"Invalid socket or callback"};
    }
    handleAddPollable(std::move(socket), std::move(callback));
}

void Poller::removePollable(zmq::socket_t& socket)
{
    handleRemovePollable(socket);
}

}
}
