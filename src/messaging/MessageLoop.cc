#include "messaging/MessageLoop.hh"
#include "Utility.hh"
#include "Zip.hh"

#include <atomic>
#include <cassert>
#include <queue>
#include <vector>
#include <utility>

namespace Bridge {
namespace Messaging {

class MessageLoop::Impl {
public:
    void addSocket(
        std::shared_ptr<zmq::socket_t> socket, SocketCallback callback);
    void addSimpleCallback(SimpleCallback callback);
    bool run();
    void terminate();
    bool isTerminated();

private:
    std::atomic<bool> terminated {false};
    std::vector<zmq::pollitem_t> pollitems;
    std::vector<
        std::pair<SocketCallback, std::shared_ptr<zmq::socket_t>>> callbacks;
    std::queue<SimpleCallback> simpleCallbacks;
};

void MessageLoop::Impl::addSocket(
    std::shared_ptr<zmq::socket_t> socket, SocketCallback callback)
{
    pollitems.emplace_back(
        zmq::pollitem_t {
            static_cast<void*>(dereference(socket)), 0, ZMQ_POLLIN, 0});
    try {
        callbacks.emplace_back(std::move(callback), std::move(socket));
    } catch (...) {
        pollitems.pop_back();
        throw;
    }
}

void MessageLoop::Impl::addSimpleCallback(SimpleCallback callback)
{
    simpleCallbacks.emplace(std::move(callback));
}

bool MessageLoop::Impl::run()
{
    for (auto& item : pollitems) {
        item.revents = 0;
    }
    static_cast<void>(zmq::poll(pollitems));
    for (auto&& t : zip(pollitems, callbacks)) {
        const auto& item = t.get<0>();
        auto& callback = t.get<1>();
        if (item.revents & ZMQ_POLLIN) {
            assert(callback.second);
            const auto go = callback.first(*callback.second);
            while (!simpleCallbacks.empty()) {
                simpleCallbacks.front()();
                simpleCallbacks.pop();
            }
            if (!go) {
                return false;
            }
        }
    }
    return true;
}

void MessageLoop::Impl::terminate()
{
    terminated = true;
}

bool MessageLoop::Impl::isTerminated()
{
    return terminated;
}

MessageLoop::MessageLoop() :
    impl {std::make_unique<Impl>()}
{
}

MessageLoop::~MessageLoop() = default;

void MessageLoop::addSocket(
    std::shared_ptr<zmq::socket_t> socket, SocketCallback callback)
{
    assert(impl);
    impl->addSocket(std::move(socket), std::move(callback));
}

void MessageLoop::callOnce(SimpleCallback callback)
{
    assert(impl);
    impl->addSimpleCallback(std::move(callback));
}

void MessageLoop::run()
{
    assert(impl);
    auto go = true;
    while (go && !impl->isTerminated()) {
        try {
            go = impl->run();
        } catch (const zmq::error_t& e) {
            // If receiving failed because of interrupt signal, continue
            // Otherwise rethrow
            if (e.num() != EINTR) {
                throw;
            }
        }
    }
}

void MessageLoop::terminate()
{
    assert(impl);
    impl->terminate();
}

}
}
