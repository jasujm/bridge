/** \file
 *
 * \brief Definition of Bridge::Messaging::FunctionMessageHandler class
 */

#ifndef MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
#define MESSAGING_FUNCTIONMESSAGEHANDLER_HH_

#include "MessageHandler.hh"

#include <utility>
#include <string>
#include <tuple>
#include <type_traits>

namespace Bridge {
namespace Messaging {

/** \brief Class that adapts a function into MessageHandler interface
 *
 * FunctionMessageHandler wraps any function into MessageHandler interface. It
 * is responsible for forwarding the string arguments to the handler to the
 * function in type safe manner, as well as converting its return values back
 * to strings. The conversion of function parameters and return values is
 * controlled by SerializationPolicy.
 *
 * \tparam SerializationPolicy Type for an object that controls conversion of
 * function parameters and return values. It needs to have two template
 * functions:
 *   - \p serialize which must accept an object returned by the underlying
 *     function and return a string representation of it. The type accepted is
 *     one of the return values of the function.
 *   - \p deserialize which must accept a string and return object which the
 *     parameter represents. The type is given to the method as first template
 *     argument and is one of the types passed to the function as argument.
 *
 * \tparam Function Type of the function that FunctionMessageHandler
 * wraps. The function may take an arbitrary number of parameters and return
 * an arbitrary sized instance of std::tuple containing the return values.
 *
 * \tparam Args The types of the arguments used when calling the
 * function. This needs to be specified to distinguish between different
 * possible calling signatures provided by the function.
 */
template<typename SerializationPolicy, typename Function, typename... Args>
class FunctionMessageHandler : public MessageHandler {
public:

    /** \brief Create function message handler
     *
     * \param function the function used to execute the action
     * \param serializer the SerializationPolicy object used to serialize and
     * deserialize strings
     */
    explicit FunctionMessageHandler(
        Function function,
        SerializationPolicy serializer = {});

private:

    ReturnValue handle(ParameterRange params) override;

     template<std::size_t... Ns, std::size_t... Ms>
    ReturnValue callFunction(
        ParameterRange params, std::index_sequence<Ns...>,
        std::index_sequence<Ms...>)
    {
        auto ret = function(
            serializer.template deserialize<Args>(params[Ns])...);
        // to silence compiler warning if the return value is empty
        static_cast<void>(ret);
        return {serializer.serialize(std::get<Ms>(ret))...};
    }

    Function function;
    SerializationPolicy serializer;
};

template<typename SerializationPolicy, typename Function, typename... Args>
FunctionMessageHandler<SerializationPolicy, Function, Args...>::FunctionMessageHandler(
    Function function, SerializationPolicy serializer) :
    function {std::move(function)},
    serializer {std::move(serializer)}
{
}

template<typename SerializationPolicy, typename Function, typename... Args>
MessageHandler::ReturnValue
FunctionMessageHandler<SerializationPolicy, Function, Args...>::handle(
    MessageHandler::ParameterRange params)
{
    constexpr auto n_ret = std::tuple_size<
        std::result_of_t<Function(Args...)>>::value;
    return callFunction(
        params,
        std::index_sequence_for<Args...>(),
        std::make_index_sequence<n_ret>());
}

/** \brief Utility for wrapping function into FunctionMessageHandler
 *
 * Like direct constructor call to FunctionMessageHandler, but deduces
 * function type.
 *
 * \param function the function to be forwarded to FunctionMessageHandler
 * constructor
 * \param serializer the serializer to be forwarded to FunctionMessageHandler
 * constructor
 *
 * \return the constructed FunctionMessageHandler object
 *
 * \sa FunctionMessageHandler
 */
template<typename SerializationPolicy, typename... Args, typename Function>
auto makeFunctionMessageHandler(
    Function&& function, SerializationPolicy&& serializer = {})
{
    return FunctionMessageHandler<SerializationPolicy, Function, Args...> {
        std::forward<Function>(function),
        std::forward<SerializationPolicy>(serializer)};
}

}
}

#endif // MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
