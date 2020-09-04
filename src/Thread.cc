#include "Thread.hh"

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

}
