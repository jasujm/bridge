/** \file
 *
 * \brief Definition of Bridge::FunctionObserver
 */

#include "Observer.hh"

#include <memory>
#include <utility>

namespace Bridge {

/** \brief Wrapper for making any function Observer
 *
 * FunctionObserver is wrapper whose handleNotify() calls underlying function
 * given when constructing the object with the parameters passed to
 * handleNotify().
 *
 * \tparam Function type of the underlying function
 * \tparam T observer parameters
 */
template<typename Function, typename... T>
class FunctionObserver : public Observer<T...> {
public:

    /** \brief Create new function observer
     *
     * \param function the function tied to the observer
     */
    FunctionObserver(Function function);

private:

    void handleNotify(const T&... args) override;

    Function function;
};

template<typename Function, typename... T>
FunctionObserver<Function, T...>::FunctionObserver(Function function) :
    function {function}
{
}

template<typename Function, typename... T>
void FunctionObserver<Function, T...>::handleNotify(const T&... args)
{
    function(args...);
}

/** \brief Utility for wrapping a function into Observer
 *
 * This function constructs std::shared_ptr to FunctionObserver wrapping \p
 * function.
 *
 * \param function the function to wrap into Observer
 *
 * \return the constructed observer
 *
 * \sa FunctionObserver
 */
template<typename... T, typename Function>
auto makeObserver(Function&& function)
{
    return std::make_shared<FunctionObserver<Function, T...>>(
        std::forward<Function>(function));
}

}
