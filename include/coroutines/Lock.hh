/** \file
 *
 * \brief Definition of Bridge::Coroutines::Lock
 */

#ifndef COROUTINES_LOCK_HH_
#define COROUTINES_LOCK_HH_

#include <boost/noncopyable.hpp>

#include <memory>
#include <queue>

namespace Bridge {
namespace Coroutines {

class AsynchronousExecutionContext;
class Future;

/** \brief Mutual exclusion for coroutines
 *
 * A coroutine mutex is a synchronization primitive that can be used to ensure
 * only one coroutine is executing a critical section at a time.
 *
 * A coroutine mutex works on FIFO basis. While acquiring standard mutex by
 * different threads do not generally have happens‐before relationship,
 * acquiring a coroutine mutex, being a sequential operation, does. If multiple
 * coroutines are waiting for the same mutex, they are guaranteed to acquire it
 * in the same order they called \ref lock().
 *
 * \warning A coroutine mutex is not an inter‐thread synchronization mechanism,
 * nor is it thread safe. Locking a mutex from two different thread leads to
 * undefined behavior.
 *
 * \sa Lock
 */
class Mutex : private boost::noncopyable {
public:

    /** \brief Create new mutex
     *
     * The mutex is initially unlocked.
     */
    Mutex();

    /** \brief Acquire lock
     *
     * Only one coroutine can have the lock at a time. If no other coroutine has
     * the lock, the execution proceeds immediately. If another coroutine has
     * the lock, the caller is suspended until the lock is released.
     *
     * \param context the execution context
     */
    void lock(AsynchronousExecutionContext& context);

    /** \brief Release lock
     *
     * This call resumes the next coroutine awaiting the lock, if any.
     */
    void unlock();

private:

    bool locked;
    std::queue<std::shared_ptr<Future>> awaitors;
};

/** \brief RAII guard for acquiring and releasing Mutex
 */
class Lock : private boost::noncopyable {
public:

    /** \brief Acquire \p mutex
     *
     * \param context the execution context
     * \param mutex the mutex to lock
     */
    Lock(AsynchronousExecutionContext& context, Mutex& mutex);

    /** \brief Release the mutex that was locked by the constructor
     */
    ~Lock();

private:

    Mutex& mutex;
};

}
}

#endif // COROUTINES_LOCK_HH_
