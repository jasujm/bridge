#include "messaging/MessageLoop.hh"
#include "Utility.hh"
#include "Zip.hh"

#include <atomic>
#include <cassert>
#include <vector>
#include <utility>

namespace Bridge {
namespace Messaging {

class MessageLoop::Impl {
public:
    void addSocket(std::shared_ptr<zmq::socket_t> socket, Callback callback);
    bool run();
    void terminate();
    bool isTerminated();

private:
    std::atomic<bool> terminated {false};
    std::vector<zmq::pollitem_t> pollitems;
    std::vector<std::pair<Callback, std::shared_ptr<zmq::socket_t>>> callbacks;
};

void MessageLoop::Impl::addSocket(
    std::shared_ptr<zmq::socket_t> socket, Callback callback)
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
            if (!callback.first(*callback.second)) {
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
    std::shared_ptr<zmq::socket_t> socket, Callback callback)
{
    assert(impl);
    impl->addSocket(std::move(socket), std::move(callback));
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
