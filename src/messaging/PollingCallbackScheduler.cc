#include "messaging/PollingCallbackScheduler.hh"
#include "messaging/MessageUtility.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <queue>
#include <sstream>

namespace Bridge {
namespace Messaging {

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

// PollingCallbackScheduler workflow:
// - callOnce() -> store callback and send message to worker thread for waiting
// - worker thread -> wait until it's time to execute a callback, send its id
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
    zmq::context_t& context, const std::string& bAddr, const std::string& fAddr,
    zmq::socket_t terminationSubscriber)
{
    auto queue = std::priority_queue<ScheduledCallback> {};
    auto fs = zmq::socket_t {context, zmq::socket_type::pair};
    auto bs = zmq::socket_t {context, zmq::socket_type::pair};
    fs.connect(fAddr);
    bs.connect(bAddr);
    // Synchronize
    static_cast<void>(discardMessage(fs));
    bs.send("", 0);
    auto pollitems = std::array {
        zmq::pollitem_t {
            static_cast<void*>(terminationSubscriber), 0, ZMQ_POLLIN, 0 },
        zmq::pollitem_t { static_cast<void*>(fs), 0, ZMQ_POLLIN, 0 },
    };
    while (true) {
        // Poll until it's time to execute the next callback
        const auto next_timeout = millisecondsUntilCallback(queue);
        static_cast<void>(zmq::poll(pollitems.data(), pollitems.size(), next_timeout));
        // Termination notification
        if (pollitems[0].revents & ZMQ_POLLIN) {
            break;
        }
        // Poll didn't timeout -- there is new callback info to be received
        else if (pollitems[1].revents & ZMQ_POLLIN) {
            const auto now = Clk::now();
            while (fs.getsockopt<std::uint32_t>(ZMQ_EVENTS) & ZMQ_POLLIN) {
                auto info = CallbackInfo {};
                [[maybe_unused]] const auto n_recv = fs.recv(&info, sizeof(info));
                assert(n_recv == sizeof(info));
                // No need to schedule anything if there is no timeout
                if (info.timeout == Ms::zero()) {
                    bs.send(&info.callback_id, sizeof(info.callback_id), ZMQ_DONTWAIT);
                } else {
                    const auto callback_time = now + info.timeout;
                    queue.emplace(
                        ScheduledCallback {callback_time, info.callback_id});
                }
            }
        }
        // Poll did timeout -- it's time to execute the next callback by sending
        // its id back to PollingCallbackScheduler
        else {
            assert(!queue.empty());
            const auto callback_id = queue.top().callback_id;
            queue.pop();
            bs.send(&callback_id, sizeof(callback_id), ZMQ_DONTWAIT);
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

PollingCallbackScheduler::PollingCallbackScheduler(
    zmq::context_t& context, zmq::socket_t terminationSubscriber) :
    frontSocket {context, zmq::socket_type::pair},
    backSocket {
        std::make_shared<zmq::socket_t>(context, zmq::socket_type::pair)},
    callbacks {}
{
    auto back_name = generateSocketName("csbs", this);
    auto front_name = generateSocketName("csfs", this);
    backSocket->bind(back_name);
    frontSocket.bind(front_name);
    worker = Thread {
        callbackSchedulerWorker, std::ref(context), std::move(back_name),
        std::move(front_name), std::move(terminationSubscriber) };
    // Synchronize
    frontSocket.send("", 0);
    static_cast<void>(discardMessage(*backSocket));
}

void PollingCallbackScheduler::handleCallLater(
    const Ms timeout, Callback callback)
{
    static unsigned long counter {};
    callbacks.emplace(counter, std::move(callback));
    auto info = CallbackInfo {timeout, counter};
    frontSocket.send(&info, sizeof(info), ZMQ_DONTWAIT);
    ++counter;
}

std::shared_ptr<zmq::socket_t> PollingCallbackScheduler::getSocket()
{
    return backSocket;
}

void PollingCallbackScheduler::operator()(zmq::socket_t& socket)
{
    if (&socket == backSocket.get()) {
        while (socket.getsockopt<std::uint32_t>(ZMQ_EVENTS) & ZMQ_POLLIN) {
            unsigned long callback_id {};
            [[maybe_unused ]]const auto n_recv =
                socket.recv(&callback_id, sizeof(callback_id));
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
