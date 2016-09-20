/** \file
 *
 * \brief Definition of Bridge::FunctionQueue class
 */

#ifndef FUNCTIONQUEUE_HH_
#define FUNCTIONQUEUE_HH_

#include <functional>
#include <list>

namespace Bridge {

/** \brief Function queue
 *
 * FunctionQueue is a class which helps calling series of functions that are
 * non‐reentrant or whose calls must not overlap for some other reason. It
 * works by accepting function objects and calling them. If the functions
 * require other functions to be called, the functions can be queued and
 * executed once the previous function has returned.
 */
class FunctionQueue {
public:

    /** \brief Enqueue function for execution
     *
     * If the queue is currently empty, the function is immediately
     * called. Otherwise it is enqueued and called once the previous functions
     * have returned. If any of the function throws an exception, the queue is
     * cleared before the exception is rethrown.
     *
     * \note If \p function stores values with automatic storage duration by
     * reference, the client of this class must ensure that there are no
     * functions in the queue when this method is called.
     *
     * \param function The function to enqueue. It must be invoked without
     * arguments. The return value of the function is ignored.
     */
    template<typename Function>
    void operator()(Function&& function);

private:

    void internalProcessQueue();

    std::list<std::function<void()>> functions;
};

template<typename Function>
void FunctionQueue::operator()(Function&& function)
{
    try {
        functions.emplace_back(std::forward<Function>(function));
        if (functions.size() == 1) {
            internalProcessQueue();
        }
    } catch(...) {
        functions.clear();
        throw;
    }
}

inline void FunctionQueue::internalProcessQueue()
{
    while (!functions.empty()) {
        (functions.front())();
        functions.pop_front();
    }
}

}

#endif // FUNCTIONQUEUE_HH_
