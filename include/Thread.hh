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
 * This thread class wraps std::thread with the following additional
 * functionality
 *
 * - The thread is automatically joined in the destructor
 * - The thread blocks SIGINT and SIGTERM signals
 *
 * This class is used for worker threads. The main thread should handle SIGINT
 * and SIGTERM.
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

    /** \brief Join the thread, if joinable
     */
    ~Thread();

    /** \brief Move assignment
     *
     * Move assigns thread from \p other. Leaves \p *this object empty.
     */
    Thread& operator=(Thread&& other) noexcept;

private:

    static void blockSignals();

    std::thread thread;
};

template<typename Function, typename... Args>
Thread::Thread(Function&& f, Args&&... args) :
    thread {
        [f = std::forward<Function>(f)](auto&&... args)
        {
            blockSignals();
            std::invoke(std::move(f), std::move(args)...);
        }, std::forward<Args>(args)...}
{
}

}  // Bridge

#endif // THREAD_HH_
