#include "messaging/PollingCallbackScheduler.hh"
#include "messaging/MessageUtility.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <queue>
#include <sstream>

using namespace std::chrono_literals;

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

using CallbackQueue = std::priority_queue<ScheduledCallback>;

// PollingCallbackScheduler workflow:
// - callOnce() -> store callback and send message to worker thread for waiting
// - worker thread -> wait until it's time to execute a callback, send its id
//   back to main thread
// - operator()() -> when the back socket receives message, callback is executed

auto millisecondsUntilCallback(const CallbackQueue& queue)
{
    if (queue.empty()) {
        return -1ms;
    }
    return std::max(
        0ms, std::chrono::duration_cast<Ms>(queue.top().time_to_execute - Clk::now()));
}

bool operator<(const ScheduledCallback& cb1, const ScheduledCallback& cb2)
{
    return cb1.time_to_execute > cb2.time_to_execute;
}

void callbackSchedulerWorker(
    MessageContext& context, const std::string& bAddr, const std::string& fAddr,
    Socket terminationSubscriber)
{
    auto queue = CallbackQueue {};
    auto fs = Socket {context, SocketType::pair};
    auto bs = Socket {context, SocketType::pair};
    connectSocket(fs, fAddr);
    connectSocket(bs, bAddr);
    // Synchronize
    static_cast<void>(discardMessage(fs));
    sendEmptyMessage(bs);
    auto pollitems = std::array {
        Pollitem { terminationSubscriber.handle(), 0, ZMQ_POLLIN, 0 },
        Pollitem { fs.handle(), 0, ZMQ_POLLIN, 0 },
    };
    while (true) {
        // Poll until it's time to execute the next callback
        const auto next_timeout = millisecondsUntilCallback(queue);
        pollSockets(pollitems, next_timeout);
        // Termination notification
        if (pollitems[0].revents & ZMQ_POLLIN) {
            break;
        }
        // Poll didn't timeout -- there is new callback info to be received
        else if (pollitems[1].revents & ZMQ_POLLIN) {
            const auto now = Clk::now();
            while (socketHasEvents(fs, ZMQ_POLLIN)) {
                auto info = CallbackInfo {};
                [[maybe_unused]] const auto recv_result = recvMessage(
                    fs, messageBuffer(&info, sizeof(info)));
                assert(!recv_result.truncated());
                // No need to schedule anything if there is no timeout
                if (info.timeout == 0ms) {
                    const auto buffer = messageBuffer(
                        &info.callback_id, sizeof(info.callback_id));
                    static_cast<void>(sendMessageNonblocking(bs, buffer));
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
            const auto cbid = queue.top().callback_id;
            const auto buffer = messageBuffer(&cbid, sizeof(cbid));
            queue.pop();
            static_cast<void>(sendMessageNonblocking(bs, buffer));
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
    MessageContext& context, Socket terminationSubscriber) :
    frontSocket {context, SocketType::pair},
    backSocket {makeSharedSocket(context, SocketType::pair)},
    callbacks {}
{
    auto back_name = generateSocketName("csbs", this);
    auto front_name = generateSocketName("csfs", this);
    bindSocket(*backSocket, back_name);
    bindSocket(frontSocket, front_name);
    worker = Thread {
        callbackSchedulerWorker, std::ref(context), std::move(back_name),
        std::move(front_name), std::move(terminationSubscriber) };
    // Synchronize
    sendEmptyMessage(frontSocket);
    static_cast<void>(discardMessage(*backSocket));
}

void PollingCallbackScheduler::handleCallLater(
    const Ms timeout, Callback callback)
{
    static unsigned long counter {};
    callbacks.emplace(counter, std::move(callback));
    auto info = CallbackInfo {timeout, counter};
    const auto buffer = messageBuffer(&info, sizeof(info));
    static_cast<void>(sendMessageNonblocking(frontSocket, buffer));
    ++counter;
}

SharedSocket PollingCallbackScheduler::getSocket()
{
    return backSocket;
}

void PollingCallbackScheduler::operator()(Socket& socket)
{
    if (&socket == backSocket.get()) {
        while (socketHasEvents(socket, ZMQ_POLLIN)) {
            unsigned long callback_id {};
            [[maybe_unused]] const auto recv_result = recvMessage(
                socket, messageBuffer(&callback_id, sizeof(callback_id)));
            assert(!recv_result.truncated());
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
