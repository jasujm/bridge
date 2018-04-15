#include "main/CallbackScheduler.hh"

#include <sstream>

namespace Bridge {
namespace Main {

CallbackScheduler::CallbackScheduler(zmq::context_t& context) :
    frontSocket {context, zmq::socket_type::pair},
    backSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)},
    callbacks {}
{
    std::ostringstream os;
    os << "inproc://" << this;
    backSocket->bind(os.str());
    frontSocket.connect(os.str());
}

void CallbackScheduler::callOnce(Callback callback)
{
    callbacks.emplace(std::move(callback));
    frontSocket.send(zmq::message_t {});
}

std::shared_ptr<zmq::socket_t> CallbackScheduler::getSocket()
{
    return backSocket;
}

void CallbackScheduler::operator()(zmq::socket_t& socket)
{
    if (&socket == backSocket.get()) {
        while (socket.getsockopt<std::uint32_t>(ZMQ_EVENTS) & ZMQ_POLLIN) {
            zmq::message_t msg {};
            const auto success = socket.recv(&msg);
            assert(success);
            assert(!callbacks.empty());
            const auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback();
        }
    }
}

}
}
