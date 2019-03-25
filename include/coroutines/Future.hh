#ifndef COROUTINES_FUTURE_HH_
#define COROUTINES_FUTURE_HH_

#include <functional>

namespace Bridge {
namespace Coroutines {

class CoroutineAdapter;

/** \brief Future that a coroutine can await
 *
 * When awaited by CoroutineAdapter, the coroutine will be resumed when the
 * future is completed by another (co)routine.
 *
 * \note The class is so named because it resembles the well‐known future
 * concept. As of now only “void” futures are supported, i.e. the coroutine
 * can be notified but no value can be transferred using it.
 */
class Future
{
public:

    /** \brief Default constructor
     *
     * Create a future that is initially not awaited by any coroutine.
     */
    Future();

    /** \brief Resolve the future
     *
     * If a coroutine function associated with a \ref CoroutineAdapter was
     * awaiting this future, it is resumed.
     */
    void resolve();

private:

    static void nullResolve();

    std::function<void()> resolveCallback;

    friend class CoroutineAdapter;
};

}
}

#endif // COROUTINES_FUTURE_HH_
