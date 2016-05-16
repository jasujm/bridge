#include "messaging/MessageLoop.hh"
#include "Zip.hh"

#include <atomic>
#include <cassert>
#include <vector>
#include <utility>

namespace Bridge {
namespace Messaging {

class MessageLoop::Impl {
public:
    void addSocket(zmq::socket_t& socket, Callback callback);
    void run();
    void terminate();

private:
    std::atomic<bool> go {true};
    std::vector<zmq::pollitem_t> pollitems;
    std::vector<
        std::pair<Callback, std::reference_wrapper<zmq::socket_t>>> callbacks;
};

void MessageLoop::Impl::addSocket(zmq::socket_t& socket, Callback callback)
{
    pollitems.emplace_back(
        zmq::pollitem_t {static_cast<void*>(socket), 0, ZMQ_POLLIN, 0});
    callbacks.emplace_back(std::move(callback), socket);
}

void MessageLoop::Impl::run()
{
    auto local_go = true;
    while (local_go && go) {
        for (auto& item : pollitems) {
            item.revents = 0;
        }
        static_cast<void>(zmq::poll(pollitems));
        for (auto&& t : zip(pollitems, callbacks)) {
            const auto& item = t.get<0>();
            auto& callback = t.get<1>();
            if (item.revents & ZMQ_POLLIN) {
                local_go = local_go && callback.first(callback.second);
            }
        }
    }
}

void MessageLoop::Impl::terminate()
{
    go = false;
}

MessageLoop::MessageLoop() :
    impl {std::make_unique<Impl>()}
{
}

MessageLoop::~MessageLoop() = default;

void MessageLoop::addSocket(zmq::socket_t& socket, Callback callback)
{
    assert(impl);
    impl->addSocket(socket, std::move(callback));
}

void MessageLoop::run()
{
    assert(impl);
    try {
        impl->run();
    } catch (const zmq::error_t& e) {
        // If receiving failed because of interrupt signal, terminate
        if (e.num() == EINTR) {
            impl->terminate();
        }
        // Otherwise rethrow the exception
        else {
            throw;
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
