/** \file
 *
 * \brief Definition of Bridge::Messaging::FunctionMessageHandler class
 */

#ifndef MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
#define MESSAGING_FUNCTIONMESSAGEHANDLER_HH_

#include "messaging/MessageHandler.hh"
#include "messaging/SerializationUtility.hh"

#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Class that adapts a function into MessageHandler interface
 *
 * FunctionMessageHandler wraps any function into MessageHandler interface. It
 * is responsible for forwarding the string arguments to the handler to the
 * function in type safe manner, as well as forwarding the return value of the
 * function to report failures. The deserialization of function parameters is
 * controlled by serialization policy.
 *
 * FunctionMessageHandler reports failure by returning false from handle() if
 * the number of parameters does not match the argument count of the function
 * or if deserialization of any parameter fails. The wrapped function also
 * needs to return a value convertible to bool to indicate whether or not its
 * invocation was successful.
 *
 * \tparam Function Type of the function that FunctionMessageHandler
 * wraps. The function must take the identity of the sender of the message
 * (std::string or something to which it can be converted) as it’s first
 * argument, and may take an arbitrary number of parameters after that. It
 * must return a value convertible to bool indicating whether or not the
 * command was successfully handled.
 *
 * \tparam SerializationPolicy See \ref serializationpolicy
 *
 * \tparam Args The types of the arguments used when calling the function
 * (excluding the first identity parameter). This needs to be specified to
 * distinguish between different possible calling signatures provided by the
 * function.
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

    bool doHandle(const std::string& identity, ParameterRange params) override;

    template<std::size_t... Ns>
    bool internalCallFunction(
        const std::string& identity, ParameterRange params,
        std::index_sequence<Ns...>)
    {
        auto&& params_ = deserializeAll<std::decay_t<Args>...>(
            serializer, params[Ns]...);
        if (params_) {
            return function(identity, std::move(std::get<Ns>(*params_))...);
        }
        return false;
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
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::doHandle(
    const std::string& identity, MessageHandler::ParameterRange params)
{
    if (params.size() != sizeof...(Args)) {
        return false;
    }

    return internalCallFunction(
        identity, params, std::index_sequence_for<Args...>());
}

/** \brief Utility for wrapping function into message handler
 *
 * This function constructs FunctionMessageHandler to the heap using the
 * parameters passed to this function and returns pointer to it.
 *
 * \tparam Args the parameter types to the function, excluding the leading
 * identity string
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

/** \brief Utility for wrapping member function call into message handler
 *
 * This function constructs FunctionMessageHandler to heap. The message
 * handler invokes method call to \p memfn bound to \p handler.
 *
 * \param handler the handler object the method call is bound to \param memfn
 * pointer to the member function \param serializer the serializer used for
 * converting to/from argument and return types of the method call
 *
 * \return the constructed message handler
 *
 * \sa FunctionMessageHandler
 */
template<
    typename Handler, typename Bool, typename String, typename... Args,
    typename SerializationPolicy>
auto makeMessageHandler(
    Handler& handler, Bool (Handler::*memfn)(String, Args...),
    SerializationPolicy&& serializer)
{
    // Identity always comes as const std::string& from
    // FunctionMessageHandler::doHandle so no need to move/forward
    // it. However, other parameters are constructed in doHandle and moved to
    // the handler function, so there might be performance to be gained by
    // accepting them as rvalue references.
    return makeMessageHandler<Args...>(
        [&handler, memfn](
            const std::string& identity,
            std::add_rvalue_reference_t<Args>... args)
        {
            return (handler.*memfn)(identity, std::move(args)...);
        }, std::forward<SerializationPolicy>(serializer));
}

}
}

#endif // MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
