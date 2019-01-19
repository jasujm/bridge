#include "messaging/MessageLoop.hh"
#include "Logging.hh"
#include "Utility.hh"

#include <cassert>
#include <cstdlib>
#include <queue>
#include <sstream>
#include <vector>
#include <utility>

#include <signal.h>
#include <sys/signalfd.h>

namespace Bridge {
namespace Messaging {

namespace {

class SignalGuard {
public:
    SignalGuard();
    ~SignalGuard();

    int getSfd();

private:
    sigset_t mask {};
    sigset_t oldMask {};
    int sfd {};
};

SignalGuard::SignalGuard()
{
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    errno = 0;
    if (sigprocmask(SIG_BLOCK, &mask, &oldMask) != 0) {
        log(LogLevel::FATAL, "Failed to set sigprocmask: %s", strerror(errno));
        std::exit(EXIT_FAILURE);
    }
    sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) {
        log(LogLevel::FATAL, "Failed to create signalfd: %s", strerror(errno));
        std::exit(EXIT_FAILURE);
    }
}

SignalGuard::~SignalGuard()
{
    errno = 0;
    if (close(sfd) != 0) {
        log(LogLevel::ERROR, "Failed to close signalfd: %s", strerror(errno));
    }
    if (sigprocmask(SIG_SETMASK, &oldMask, nullptr) != 0) {
        log(LogLevel::ERROR, "Failed to restore sigprocmask: %s",
            strerror(errno));
    }
}

int SignalGuard::getSfd()
{
    return sfd;
}

}

class MessageLoop::Impl {
public:
    Impl(zmq::context_t& context);

    void addSocket(
        std::shared_ptr<zmq::socket_t> socket, SocketCallback callback);
    bool run(int sfd);
    zmq::socket_t createTerminationSubscriber();

private:

    std::string getTerminationPubSubEndpoint();

    zmq::context_t& context;
    zmq::socket_t terminationPublisher;
    std::vector<zmq::pollitem_t> pollitems {
        { nullptr, 0, ZMQ_POLLIN, 0 }
    };
    std::vector<
        std::pair<SocketCallback, std::shared_ptr<zmq::socket_t>>> callbacks;
};

MessageLoop::Impl::Impl(zmq::context_t& context) :
    context {context},
    terminationPublisher {context, zmq::socket_type::pub}
{
    terminationPublisher.bind(getTerminationPubSubEndpoint());
}

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

bool MessageLoop::Impl::run(int sfd)
{
    assert(!pollitems.empty());
    pollitems[0].fd = sfd;
    static_cast<void>(zmq::poll(pollitems));
    if (pollitems[0].revents & ZMQ_POLLIN) {
        auto fdsi = signalfd_siginfo {};
        errno = 0;
        const auto s = read(sfd, &fdsi, sizeof(signalfd_siginfo));
        if (s != sizeof(signalfd_siginfo)) {
            log(LogLevel::FATAL, "Failed to read siginfo: %s",
                strerror(errno));
            std::exit(EXIT_FAILURE);
        }
        log(LogLevel::DEBUG, "Signal received: %s", strsignal(fdsi.ssi_signo));
        if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGTERM) {
            terminationPublisher.send(&fdsi.ssi_signo, sizeof(fdsi.ssi_signo));
            return false;
        }
    }
    for (auto i = 0u; i < callbacks.size(); ++i) {
        assert(i+1 < pollitems.size());
        const auto& item = pollitems[i+1];
        auto& callback = callbacks[i];
        if (item.revents & ZMQ_POLLIN) {
            assert(callback.second);
            callback.first(*callback.second);
        }
    }
    return true;
}

zmq::socket_t MessageLoop::Impl::createTerminationSubscriber()
{
    auto subscriber = zmq::socket_t {context, zmq::socket_type::sub};
    subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    subscriber.connect(getTerminationPubSubEndpoint());
    return subscriber;
}

std::string MessageLoop::Impl::getTerminationPubSubEndpoint()
{
    std::ostringstream os;
    os << "inproc://bridge.messageloop.term." << this;
    return os.str();
}

MessageLoop::MessageLoop(zmq::context_t& context) :
    impl {std::make_unique<Impl>(context)}
{
}

MessageLoop::~MessageLoop() = default;

void MessageLoop::addSocket(
    std::shared_ptr<zmq::socket_t> socket, SocketCallback callback)
{
    assert(impl);
    impl->addSocket(std::move(socket), std::move(callback));
}

void MessageLoop::run()
{
    SignalGuard guard;
    auto go = true;
    while (go) {
        try {
            assert(impl);
            go = impl->run(guard.getSfd());
        } catch (const zmq::error_t& e) {
            // If receiving failed because of interrupt signal, continue
            // Otherwise rethrow
            if (e.num() != EINTR) {
                throw;
            }
        }
    }
}

zmq::socket_t MessageLoop::createTerminationSubscriber()
{
    assert(impl);
    return impl->createTerminationSubscriber();
}

}
}
