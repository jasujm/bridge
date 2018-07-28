#include "main/CallbackScheduler.hh"

#include <algorithm>
#include <cassert>
#include <queue>
#include <sstream>

namespace Bridge {
namespace Main {

namespace  {

using Clk = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;
using Time = std::chrono::time_point<Clk>;

struct CallbackInfo {
    Ms timeout;
    unsigned long callback_id;
};

struct ScheduledCallback {
    Time time_to_execute;
    unsigned long callback_id;
};

// CallbackScheduler workflow:
// - callOnce() -> store callback and send message to worker thread for waiting
// - worker thread -> wait until it's time to execute a callback, send it's id
//   back to main thread
// - operator()() -> when the back socket receives message, callback is executed

auto millisecondsUntilCallback(const auto& queue)
{
    if (queue.empty()) {
        return -1L;
    }
    return std::max(
        0L,
        std::chrono::duration_cast<Ms>(
            queue.top().time_to_execute - Clk::now()).count());
}

bool operator<(const ScheduledCallback& cb1, const ScheduledCallback& cb2)
{
    return cb1.time_to_execute > cb2.time_to_execute;
}

void callbackSchedulerWorker(
    zmq::context_t& context, const std::string& bAddr, const std::string& fAddr)
{
    auto queue = std::priority_queue<ScheduledCallback> {};
    auto fs = zmq::socket_t {context, zmq::socket_type::pair};
    auto bs = zmq::socket_t {context, zmq::socket_type::pair};
    fs.connect(fAddr);
    bs.connect(bAddr);
    auto pollitem = zmq::pollitem_t { static_cast<void*>(fs), 0, ZMQ_POLLIN, 0 };
    while (true) {
        // Poll until it's time to execute the next callback
        const auto next_timeout = millisecondsUntilCallback(queue);
        static_cast<void>(zmq::poll(&pollitem, 1, next_timeout));
        // Poll didn't timeout -- there is new callback info to be received
        if (pollitem.revents & ZMQ_POLLIN) {
            const auto now = Clk::now();
            while (fs.getsockopt<std::uint32_t>(ZMQ_EVENTS) & ZMQ_POLLIN) {
                auto info = CallbackInfo {};
                const auto n_recv = fs.recv(&info, sizeof(info));
                // Zero length message is the signal for terminating the thread
                if (n_recv == 0) {
                    return;
                }
                // No need to schedule anything if there is no timeout
                if (info.timeout == Ms::zero()) {
                    bs.send(&info.callback_id, sizeof(info.callback_id));
                } else {
                    const auto callback_time = now + info.timeout;
                    queue.emplace(
                        ScheduledCallback {callback_time, info.callback_id});
                }
            }
        }
        // Poll did timeout -- it's time to execute the next callback by sending
        // it's id back to CallbackScheduler
        else {
            assert(!queue.empty());
            const auto callback_id = queue.top().callback_id;
            queue.pop();
            bs.send(&callback_id, sizeof(callback_id));
        }
    }
}

std::string generateSocketName(const std::string& prefix, const void* addr)
{
    std::ostringstream os;
    os << "inproc://" << prefix << addr;
    return os.str();
}

}

CallbackScheduler::CallbackScheduler(zmq::context_t& context) :
    frontSocket {context, zmq::socket_type::pair},
    backSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)},
    callbacks {}
{
    const auto back_name = generateSocketName("csbs", this);
    const auto front_name = generateSocketName("csfs", this);
    backSocket->bind(back_name);
    frontSocket.bind(front_name);
    worker = std::thread {
        [&context, back_name, front_name]()
        {
            callbackSchedulerWorker(context, back_name, front_name);
        }
    };
}

CallbackScheduler::~CallbackScheduler()
{
    frontSocket.send(zmq::message_t {});
    worker.join();
}

void CallbackScheduler::callOnce(Callback callback, const Ms timeout)
{
    static unsigned long counter {};
    callbacks.emplace(counter, std::move(callback));
    auto info = CallbackInfo {timeout, counter};
    frontSocket.send(&info, sizeof(info));
    ++counter;
}

std::shared_ptr<zmq::socket_t> CallbackScheduler::getSocket()
{
    return backSocket;
}

void CallbackScheduler::operator()(zmq::socket_t& socket)
{
    if (&socket == backSocket.get()) {
        while (socket.getsockopt<std::uint32_t>(ZMQ_EVENTS) & ZMQ_POLLIN) {
            unsigned long callback_id {};
            const auto n_recv = socket.recv(&callback_id, sizeof(callback_id));
            assert(n_recv == sizeof(callback_id));
            const auto iter = callbacks.find(callback_id);
            if (iter != callbacks.end()) {
                const auto callback = std::move(iter->second);
                callbacks.erase(iter);
                callback();
            }
        }
    }
}

}
}
