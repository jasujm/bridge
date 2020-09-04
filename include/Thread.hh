/** \file
 *
 * \brief Definition of Bridge::Thread
 */

#ifndef THREAD_HH_
#define THREAD_HH_

#include <functional>
#include <thread>
#include <utility>

namespace Bridge {

/** \brief Thread abstraction for bridge framework
 *
 * This thread class wraps std::thread and automatically joins in the
 * destructor.
 */
class Thread {
public:

    /** \brief Default constructor
     *
     * Creates thread object not corresponding to thread of execution
     */
    Thread() noexcept;

    /** \brief Create thread running callable
     *
     * Creates thread which blocks SIGINT and SIGTERM, then calls function \p f
     * with arguments \p args
     */
    template<typename Function, typename... Args>
    Thread(Function&& f, Args&&... args);

    /** \brief Move constructor
     *
     * Move constructs thread from \p other. Leaves \p other thread empty.
     *
     * \param other the thread moved from
     */
    Thread(Thread&& other) noexcept;

    /** \brief Join the thread, if joinable
     */
    ~Thread();

    /** \brief Move assignment
     *
     * Move assigns thread from \p other. Leaves \p other thread empty.
     *
     * \param other the thread moved from
     */
    Thread& operator=(Thread&& other) noexcept;

private:

    std::thread thread;
};

template<typename Function, typename... Args>
Thread::Thread(Function&& f, Args&&... args) :
    thread {std::forward<Function>(f), std::forward<Args>(args)...}
{
}

}  // Bridge

#endif // THREAD_HH_
