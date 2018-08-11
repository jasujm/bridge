#include "Thread.hh"

#include <signal.h>

namespace Bridge {

Thread::Thread() noexcept = default;

Thread::Thread(Thread&& other) noexcept = default;

Thread::~Thread()
{
    if (thread.joinable()) {
        thread.join();
    }
}

Thread& Thread::operator=(Thread&&) noexcept = default;

void Thread::blockSignals()
{
    sigset_t mask {};
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
}

}
