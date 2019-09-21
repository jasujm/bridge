#include "messaging/MessageLoop.hh"
#include "Logging.hh"
#include "Utility.hh"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <sstream>
#include <variant>
#include <vector>
#include <utility>

#include <signal.h>
#include <sys/signalfd.h>

namespace Bridge {
namespace Messaging {

namespace {

using SocketCallbackPair = std::pair<
    Poller::SocketCallback, Poller::PollableSocket>;
using FdCallbackPair = std::pair<std::function<void(int)>, int>;
using CallbackPair = std::variant<SocketCallbackPair, FdCallbackPair>;

struct CallbackVisitor {
    void operator()(SocketCallbackPair& cb)
    {
        assert(cb.second);
        cb.first(*cb.second);
    }
    void operator()(FdCallbackPair& cb)
    {
        cb.first(cb.second);
    }
};

}

class MessageLoop::Impl {
public:
    Impl(MessageContext& context);

    void addPollable(PollableSocket socket, SocketCallback callback);
    void removePollable(Socket& socket);
    void run();
    Socket createTerminationSubscriber();

private:

    class SignalGuard {
    public:
        SignalGuard(Impl& impl);
        ~SignalGuard();

        bool go() const { return !signalReceived; }

    private:
        bool signalReceived {};
        int sfd {};
        sigset_t oldMask {};
    };

    std::string getTerminationPubSubEndpoint();

    MessageContext& context;
    Socket terminationPublisher;
    std::vector<Pollitem> pollitems {
        { nullptr, 0, ZMQ_POLLIN, 0 }
    };
    std::vector<CallbackPair> callbacks {
        FdCallbackPair {}
    };
};

MessageLoop::Impl::SignalGuard::SignalGuard(Impl& impl)
{
    sigset_t mask {};
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
    impl.pollitems[0].fd = sfd;
    auto& signal_callback = std::get<FdCallbackPair>(impl.callbacks[0]);
    signal_callback.first = [this, &impl](auto sfd) {
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
            impl.terminationPublisher.send(
                &fdsi.ssi_signo, sizeof(fdsi.ssi_signo));
            signalReceived = true;
        }
    };
    signal_callback.second = sfd;
}

MessageLoop::Impl::SignalGuard::~SignalGuard()
{
    errno = 0;
    if (close(sfd) != 0) {
        log(LogLevel::ERROR, "Failed to close signalfd: %s", strerror(errno));
    }
    errno = 0;
    if (sigprocmask(SIG_SETMASK, &oldMask, nullptr) != 0) {
        log(LogLevel::ERROR, "Failed to restore sigprocmask: %s",
            strerror(errno));
    }
}

MessageLoop::Impl::Impl(MessageContext& context) :
    context {context},
    terminationPublisher {context, SocketType::pub}
{
    terminationPublisher.bind(getTerminationPubSubEndpoint());
}

void MessageLoop::Impl::addPollable(
    PollableSocket socket, SocketCallback callback)
{
    assert(socket);
    pollitems.push_back({ static_cast<void*>(*socket), 0, ZMQ_POLLIN, 0 });
    try {
        callbacks.emplace_back(
            SocketCallbackPair {std::move(callback), std::move(socket)});
    } catch (...) {
        pollitems.pop_back();
        throw;
    }
}

void MessageLoop::Impl::removePollable(Socket& socket)
{
    const auto iter = std::find_if(
        pollitems.begin(), pollitems.end(),
        [socket_ptr = static_cast<void*>(socket)](const auto& pollitem)
        {
            return pollitem.socket == socket_ptr;
        });
    if (iter != pollitems.end()) {
        const auto n = std::distance(pollitems.begin(), iter);
        pollitems.erase(iter);
        callbacks.erase(callbacks.begin() + n);
    }
}

void MessageLoop::Impl::run()
{
    SignalGuard guard {*this};
    while (guard.go()) {
        pollSockets(pollitems);
        assert(callbacks.size() == pollitems.size());
        const auto n_callbacks = ssize(callbacks);
        for (auto i = 0; i < n_callbacks; ++i) {
            try {
                if (pollitems[i].revents & ZMQ_POLLIN) {
                    std::visit(CallbackVisitor {}, callbacks[i]);
                    break;
                }
            } catch (const SocketError& e) {
                // If receiving failed because of interrupt signal, continue
                // Otherwise rethrow
                if (e.num() != EINTR) {
                    throw;
                }
            }
        }
    }
}

Socket MessageLoop::Impl::createTerminationSubscriber()
{
    auto subscriber = Socket {context, SocketType::sub};
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

MessageLoop::MessageLoop(MessageContext& context) :
    impl {std::make_unique<Impl>(context)}
{
}

MessageLoop::~MessageLoop() = default;

void MessageLoop::handleAddPollable(
    PollableSocket socket, SocketCallback callback)
{
    assert(impl);
    impl->addPollable(std::move(socket), std::move(callback));
}

void MessageLoop::handleRemovePollable(Socket& socket)
{
    assert(impl);
    impl->removePollable(socket);
}

void MessageLoop::run()
{
    assert(impl);
    impl->run();
}

Socket MessageLoop::createTerminationSubscriber()
{
    assert(impl);
    return impl->createTerminationSubscriber();
}

}
}
