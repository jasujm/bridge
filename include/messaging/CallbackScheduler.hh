/** \file
 *
 * \brief Definition of Bridge::Messaging::CallbackScheduler
 */

#ifndef MESSAGING_CALLBACKSCHEDULER_HH_
#define MESSAGING_CALLBACKSCHEDULER_HH_

#include <chrono>
#include <functional>
#include <tuple>

namespace Bridge {
namespace Messaging {

/** \brief Interface for scheduling callbacks outside of call stack
 */
class CallbackScheduler {
public:

    virtual ~CallbackScheduler();

    /** \brief Schedule new callback
     *
     * Schedule a callable to be executed. The callback is executed outside of
     * the caller’s call stack.
     *
     * The \p callback and \p args are stored before the invocation, and are
     * required to be copyable. Invocation happens as if INVOKE, defined in
     * [func.require] in the C++ standard, is applied to the stored \p callback
     * and \p args.
     *
     * \param callable the callback to be scheduled
     * \param args the arguments to the callback
     */
    template<typename Callable, typename... Args>
    void callSoon(Callable&& callable, Args&&... args);

    /** \brief Schedule new callback
     *
     * Schedule a callable to be executed. The callback is executed outside of
     * the caller’s call stack after \p timeout has passed.
     *
     * The \p callback and \p args are stored before the invocation, and are
     * required to be copyable. Invocation happens as if INVOKE, defined in
     * [func.require] in the C++ standard, is applied to the stored \p callback
     * and \p args.
     *
     * \param callable the callback to be scheduled
     * \param args the arguments to the callback
     * \param timeout the timeout until the callback is executed
     */
    template<typename Callable, typename... Args>
    void callLater(
        std::chrono::milliseconds timeout, Callable&& callable, Args&&... args);

protected:

    /** \brief Type erased callback
     */
    using Callback = std::function<void()>;

private:

    template<typename Callable, typename... Args>
    Callback internalCreateCallback(Callable&& callable, Args&&... args);

    /** \brief Handle for callSoon()
     *
     * The default implementation calls handleCallLater() with zero timeout. May
     * be overridden for a more optimized implementation.
     *
     * The overriding implementation may only call \p callback once.
     */
    virtual void handleCallSoon(Callback callback);

    /** \brief Handle for callLater()
     *
     * The overriding implementation may only call \p callback once.
     */
    virtual void handleCallLater(
        std::chrono::milliseconds timeout, Callback callback) = 0;
};

template<typename Callable, typename... Args>
CallbackScheduler::Callback CallbackScheduler::internalCreateCallback(
    Callable&& callable, Args&&... args)
{
    return [callable = std::forward<Callable>(callable),
            args_ = std::tuple {std::forward<Args>(args)...}]() mutable
    {
        std::apply(std::move(callable), std::move(args_));
    };
}

template<typename Callable, typename... Args>
void CallbackScheduler::callSoon(Callable&& callable, Args&&... args)
{
    handleCallSoon(
        internalCreateCallback(
            std::forward<Callable>(callable), std::forward<Args>(args)...));
}

template<typename Callable, typename... Args>
void CallbackScheduler::callLater(
    std::chrono::milliseconds timeout, Callable&& callable, Args&&... args)
{
    handleCallLater(
        timeout, internalCreateCallback(
            std::forward<Callable>(callable), std::forward<Args>(args)...));
}

}
}

#endif // MESSAGING_CALLBACKSCHEDULER_HH_
