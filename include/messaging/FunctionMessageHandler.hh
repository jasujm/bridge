/** \file
 *
 * \brief Definition of Bridge::Messaging::FunctionMessageHandler class
 */

#ifndef MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
#define MESSAGING_FUNCTIONMESSAGEHANDLER_HH_

#include "messaging/MessageHandler.hh"
#include "messaging/SerializationUtility.hh"

#include <boost/variant/variant.hpp>

#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Struct that represents failed reply to message
 *
 * \sa Reply
 */
struct ReplyFailure {};

/** \brief Struct that represents successful reply to message
 *
 * A successful reply may contain an arbitrary number of arguments passed to
 * the sender of the message.
 *
 * \tparam Args the types of the arguments of the reply
 *
 * \sa Reply
 */
template<typename... Args>
struct ReplySuccess {

    /** \brief Create ReplySuccess from other type of ReplySuccess
     *
     * This constructor is used to convert between compatible ReplySuccess
     * objects.
     *
     * \param other compatible ReplySuccess object
     */
    template<typename... Args2>
    ReplySuccess(ReplySuccess<Args2...> other) :
        arguments {std::move(other.arguments)}
    {
    }

    /** \brief Create ReplySuccess from tuple
     *
     * \param args arguments that will be passed to the sender of the message
     */
    template<typename... Args2>
    explicit ReplySuccess(std::tuple<Args2...> args) :
        arguments {std::move(args)}
    {
    }

    /** \brief Arguments of the reply
     */
    std::tuple<Args...> arguments;
};

/** \brief Reply to message handled by FunctionMessageHandler
 */
template<typename... Args>
using Reply = boost::variant<ReplyFailure, ReplySuccess<Args...>>;

/** \brief Convenience function for creating successful reply
 *
 * \param args Arguments of the reply
 *
 * \return Successful reply with the given arguments
 */
template<typename... Args>
auto success(Args&&... args)
{
    return ReplySuccess<Args...> {
        std::forward_as_tuple<Args...>(std::forward<Args>(args)...)};
}

/** \brief Convenience function for creating failed reply
 *
 * \return Failed reply
 */
inline auto failure()
{
    return ReplyFailure {};
}

/** \brief Class that adapts a function into MessageHandler interface
 *
 * FunctionMessageHandler wraps any function into MessageHandler interface. It
 * is responsible for forwarding the string arguments of the handler to the
 * function in type safe manner, as well as outputting the return value of the
 * function back to the caller. The deserialization of function parameters is
 * controlled by serialization policy.
 *
 * The function that handles the message must return an object of type \ref
 * Reply. Depending on the type of the reply (ReplySuccess or ReplyFailure)
 * the message handler returns true or false to the caller. In case of
 * successful reply, any arguments of ReplySuccess are serialized and written
 * to the output iterator provided by the caller.
 *
 * The following is an example of message handler function in hypothetical
 * cloud service that checks credentials of a client and returns list of files
 * if the check passes.
 *
 * \code{.cc}
 * Reply<std::vector<std::string>> getFiles(
 *     const std::string& identity, const std::string& credentials)
 * {
 *     if (checkCredentials(identity, credentials)) {
 *         return success(getFilesForUser(identity));
 *     }
 *     return failure();
 * }
 * \endcode
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

    bool doHandle(
        const std::string& identity, ParameterRange params,
        OutputSink sink) override;

    class ReplyVisitor {
    public:

        ReplyVisitor(SerializationPolicy& serializer, OutputSink& sink);

        bool operator()(ReplyFailure) const;

        template<typename... Args2>
        bool operator()(const ReplySuccess<Args2...>& reply) const;

    private:

        template<std::size_t N = 0, typename... Args2>
        bool internalReplySuccess(const std::tuple<Args2...>& args) const;

        template<std::size_t N, typename... Args2>
        bool internalReplySuccessHelper(
            const std::tuple<Args2...>& args, std::true_type) const;

        template<std::size_t N, typename... Args2>
        bool internalReplySuccessHelper(
            const std::tuple<Args2...>& args, std::false_type) const;

        SerializationPolicy& serializer;
        OutputSink& sink;
    };

    template<std::size_t... Ns>
    bool internalCallFunction(
        const std::string& identity, ParameterRange params, OutputSink& sink,
        std::index_sequence<Ns...>)
    {
        auto&& params_ = deserializeAll<std::decay_t<Args>...>(
            serializer, params[Ns]...);
        if (params_) {
            auto&& result = function(
                identity, std::move(std::get<Ns>(*params_))...);
            return boost::apply_visitor(ReplyVisitor(serializer, sink), result);
        }
        return false;
    }

    Function function;
    SerializationPolicy serializer;
};

template<typename Function, typename SerializationPolicy, typename... Args>
FunctionMessageHandler<Function, SerializationPolicy, Args...>::
FunctionMessageHandler(
    Function function, SerializationPolicy serializer) :
    function {std::move(function)},
    serializer {std::move(serializer)}
{
}

template<typename Function, typename SerializationPolicy, typename... Args>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::doHandle(
    const std::string& identity, MessageHandler::ParameterRange params,
    OutputSink sink)
{
    if (params.size() != sizeof...(Args)) {
        return false;
    }

    return internalCallFunction(
        identity, params, sink, std::index_sequence_for<Args...>());
}

template<typename Function, typename SerializationPolicy, typename... Args>
FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::ReplyVisitor(SerializationPolicy& serializer, OutputSink& sink) :
    serializer {serializer},
    sink {sink}
{
}

template<typename Function, typename SerializationPolicy, typename... Args>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::operator()(ReplyFailure) const
{
    return false;
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<typename... Args2>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::operator()(const ReplySuccess<Args2...>& reply) const
{
    return internalReplySuccess(reply.arguments);
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<std::size_t N, typename... Args2>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::internalReplySuccess(const std::tuple<Args2...>& args) const
{
    using IsLast = typename std::integral_constant<bool, sizeof...(Args2) == N>;
    return internalReplySuccessHelper<N>(args, IsLast());
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<std::size_t N, typename... Args2>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::internalReplySuccessHelper(
    const std::tuple<Args2...>&, std::true_type) const
{
    return true;
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<std::size_t N, typename... Args2>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::internalReplySuccessHelper(
    const std::tuple<Args2...>& args, std::false_type) const
{
    sink(serializer.serialize(std::get<N>(args)));
    return internalReplySuccess<N+1>(args);
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
    typename Handler, typename Reply, typename String, typename... Args,
    typename SerializationPolicy>
auto makeMessageHandler(
    Handler& handler, Reply (Handler::*memfn)(String, Args...),
    SerializationPolicy&& serializer)
{
    // Identity always comes as const std::string& from
    // FunctionMessageHandler::doHandle so no need to move/forward
    // it. However, other parameters are constructed in doHandle and moved to
    // the handler function, so there might be performance to be gained by
    // accepting them as rvalue references.
    return makeMessageHandler<Args...>(
        [&handler, memfn](
            const std::string& identity, std::decay_t<Args>&&... args)
        {
            return (handler.*memfn)(identity, std::move(args)...);
        }, std::forward<SerializationPolicy>(serializer));
}

}
}

#endif // MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
