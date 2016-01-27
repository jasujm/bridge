/** \file
 *
 * \brief Definition of Bridge::Messaging::FunctionMessageHandler class
 */

#ifndef MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
#define MESSAGING_FUNCTIONMESSAGEHANDLER_HH_

#include "MessageHandler.hh"

#include <memory>
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
 * FunctionMessageHandler throws MessageHandlingException if the number of
 * parameters does not match the argument count of the function. The
 * serializer may throw MessageHandlingException to signal failure to
 * serialize or deserialize values. The underlying function may throw
 * MessageHandlingException to indicate failure to handle the
 * request. FunctionMessageHandler is exception neutral and does not catch any
 * exception, which means that other exceptions, including fatal ones like
 * std::bad_alloc, are propagated to the caller as is.
 *
 * \tparam Function Type of the function that FunctionMessageHandler
 * wraps. The function may take an arbitrary number of parameters and return
 * an arbitrary sized instance of std::tuple containing the return values.
 *
 * \tparam SerializationPolicy Type for an object that controls conversion of
 * function parameters and return values. It needs to have the following
 * signature:
 * \code{.cc}
 * class ExampleSerializationPolicy {
 *     // Return string representation of t
 *     template<typename T> std::string serialize(const T& t);
 *     // Return object of type T represented by s
 *     template<typename T> T deserialize(const std::string& s);
 * };
 * \endcode
 *
 * \tparam Args The types of the arguments used when calling the
 * function. This needs to be specified to distinguish between different
 * possible calling signatures provided by the function.
 *
 * \sa makeMessageHandler()
 */
template<typename Function, typename SerializationPolicy, typename... Args>
class FunctionMessageHandler : public MessageHandler {
public:

    /** \brief Create function message handler
     *
     * \param function the function used to execute the action
     * \param serializer the SerializationPolicy object used to serialize and
     * deserialize strings
     */
    FunctionMessageHandler(Function function, SerializationPolicy serializer);

private:

    ReturnValue doHandle(ParameterRange params) override;

    template<std::size_t... Ns, std::size_t... Ms>
    ReturnValue internalCallFunction(
        ParameterRange params, std::index_sequence<Ns...>,
        std::index_sequence<Ms...>)
    {
        auto&& params_ = std::make_tuple(
            serializer.template deserialize<Args>(params[Ns])...);
        auto ret = function(std::get<Ns>(std::move(params_))...);
        // to silence compiler warning if the return value is empty
        static_cast<void>(ret);
        return {serializer.serialize(std::get<Ms>(ret))...};
    }

    Function function;
    SerializationPolicy serializer;
};

template<typename Function, typename SerializationPolicy, typename... Args>
FunctionMessageHandler<Function, SerializationPolicy, Args...>::FunctionMessageHandler(
    Function function, SerializationPolicy serializer) :
    function {std::move(function)},
    serializer {std::move(serializer)}
{
}

template<typename Function, typename SerializationPolicy, typename... Args>
MessageHandler::ReturnValue
FunctionMessageHandler<Function, SerializationPolicy, Args...>::doHandle(
    MessageHandler::ParameterRange params)
{
    if (params.size() != sizeof...(Args)) {
        throw MessageHandlingException {};
    }

    constexpr auto n_ret = std::tuple_size<
        std::result_of_t<Function(Args...)>>::value;
    return internalCallFunction(
        params,
        std::index_sequence_for<Args...>(),
        std::make_index_sequence<n_ret>());
}

/** \brief Utility for wrapping function into message handler
 *
 * This function constructs FunctionMessageHandler to the heap using the
 * parameters passed to this function and returns pointer to it.
 *
 * \param function the function to be wrapped
 * \param serializer the serializer used for converting to/from argument and
 * return types of the function
 *
 * \return the constructed message handler
 *
 * \sa FunctionMessageHandler
 */
template<typename... Args, typename Function, typename SerializationPolicy>
auto makeMessageHandler(Function&& function, SerializationPolicy&& serializer)
{
    return std::make_unique<
        FunctionMessageHandler<Function, SerializationPolicy, Args...>>(
        std::forward<Function>(function),
        std::forward<SerializationPolicy>(serializer));
}

}
}

#endif // MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
