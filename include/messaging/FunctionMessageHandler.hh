/** \file
 *
 * \brief Definition of Bridge::Messaging::BasicFunctionMessageHandler class
 */

#ifndef MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
#define MESSAGING_FUNCTIONMESSAGEHANDLER_HH_

#include "messaging/Identity.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/SerializationFailureException.hh"
#include "Blob.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

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
 * \tparam Args the types of the arguments accompanied with the reply
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
        arguments(std::move(other.arguments))
    {
    }

    /** \brief Create ReplySuccess from tuple
     *
     * \param args arguments that will be passed to the sender of the message
     */
    template<typename... Args2>
    explicit ReplySuccess(std::tuple<Args2...> args) :
        arguments(std::move(args))
    {
    }

    /** \brief Arguments of the reply
     */
    std::tuple<Args...> arguments;
};

/** \brief Reply to message handled by FunctionMessageHandler
 */
template<typename... Args>
struct Reply {

    /** \brief Tuple consisting of the types accompanying the successful reply
     */
    using Types = std::tuple<Args...>;

    /** \brief Create successful reply
     *
     * \param reply the reply
     */
    template<typename... Args2>
    Reply(ReplySuccess<Args2...> reply) : reply {std::move(reply)}
    {
    }

    /** \brief Create failed reply
     *
     * \param reply the reply
     */
    Reply(ReplyFailure reply) : reply {std::move(reply)}
    {
    }

    /** \brief Variant containing the successful or the failed reply
     */
    std::variant<ReplyFailure, ReplySuccess<Args...>> reply;
};

/** \brief Convenience function for creating successful reply
 *
 * \param args Arguments of the reply
 *
 * \return Successful reply accompanied by the given arguments
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

/// \cond DOXYGEN_IGNORE

namespace FunctionMessageHandlerImpl {

template<typename T>
struct ParamWrapperImplBase {
    using WrappedType = std::optional<T>;
    using DeserializedType = T;
    static void wrap(DeserializedType&& param, WrappedType& wrapper)
        {
            wrapper.emplace(std::move(param));
        }
};

template<typename T>
struct ParamWrapperImpl : public ParamWrapperImplBase<T> {
    using Base = ParamWrapperImplBase<T>;
    using WrappedType = typename Base::WrappedType;
    using DeserializedType = typename Base::DeserializedType;
    static constexpr bool optional = false;
    static auto& unwrap(WrappedType& wrapper)
        {
            assert(wrapper);
            return *wrapper;
        }
};

template<typename T>
struct ParamWrapperImpl<std::optional<T>> : public ParamWrapperImplBase<T> {
    using Base = ParamWrapperImplBase<T>;
    using WrappedType = typename Base::WrappedType;
    using DeserializedType = typename Base::DeserializedType;
    static constexpr bool optional = true;
    static auto& unwrap(WrappedType& wrapper) { return wrapper; }
};

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
class ReplyVisitor {
public:

    ReplyVisitor(
        SerializationPolicy& serializer, Response& response,
        std::array<Blob, REPLY_SIZE>& replyKeys);

    void operator()(ReplyFailure);

    template<typename... Args2>
    void operator()(const ReplySuccess<Args2...>& reply);

private:

    template<std::size_t N, typename T>
    void internalAddFrameImpl(const T& t);

    template<std::size_t N, typename T>
    void internalAddFrame(const T& t);

    template<std::size_t N, typename T>
    void internalAddFrame(const std::optional<T>& t);

    template<typename... Args2, std::size_t... Ns>
    void internalSendReply(
        const std::tuple<Args2...>& args, std::index_sequence<Ns...>);

    SerializationPolicy& serializer;
    Response& response;
    std::array<Blob, REPLY_SIZE>& replyKeys;
};

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
ReplyVisitor<SerializationPolicy, REPLY_SIZE>::ReplyVisitor(
    SerializationPolicy& serializer, Response& response,
    std::array<Blob, REPLY_SIZE>& replyKeys) :
    serializer {serializer},
    response {response},
    replyKeys {replyKeys}
{
}

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
void ReplyVisitor<SerializationPolicy, REPLY_SIZE>::operator()(ReplyFailure)
{
    response.setStatus(REPLY_FAILURE);
}

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
template<std::size_t N, typename T>
void ReplyVisitor<SerializationPolicy, REPLY_SIZE>::
internalAddFrameImpl(const T& t)
{
    response.addFrame(std::get<N>(replyKeys));
    response.addFrame(asBytes(serializer.serialize(t)));
}

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
template<std::size_t N, typename T>
void ReplyVisitor<SerializationPolicy, REPLY_SIZE>::internalAddFrame(const T& t)
{
    internalAddFrameImpl<N>(t);
}

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
template<std::size_t N, typename T>
void ReplyVisitor<SerializationPolicy, REPLY_SIZE>::
internalAddFrame(const std::optional<T>& t)
{
    if (t) {
        internalAddFrameImpl<N>(*t);
    }
}

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
template<typename... Args2, std::size_t... Ns>
void ReplyVisitor<SerializationPolicy, REPLY_SIZE>::
internalSendReply(const std::tuple<Args2...>& args, std::index_sequence<Ns...>)
{
    ( ... , internalAddFrame<Ns>(std::get<Ns>(args)) );
}

template<typename SerializationPolicy, std::size_t REPLY_SIZE>
template<typename... Args2>
void ReplyVisitor<SerializationPolicy, REPLY_SIZE>::operator()(
    const ReplySuccess<Args2...>& reply)
{
    response.setStatus(REPLY_SUCCESS);
    internalSendReply(reply.arguments, std::index_sequence_for<Args2...> {});
}

template<typename Function, typename ExecutionContext, typename... Args>
inline constexpr auto IsInvocableWithExecutionContext = std::is_invocable_v<
    Function, ExecutionContext, const Identity&, Args...>;

template<typename Function, typename... Args>
inline constexpr auto IsInvocableWithoutExecutionContext = std::is_invocable_v<
    Function, const Identity&, Args...>;

template<
    typename ExecutionContext, typename Function, typename... Args,
    std::enable_if_t<
        IsInvocableWithExecutionContext<
            Function, ExecutionContext, Args...>, int> = 0>
decltype(auto) invokeWithExecutionContextIfPossible(
    Function&& function, ExecutionContext&& context, const Identity& identity,
    Args&&... args)
{
    return std::invoke(
        std::forward<Function>(function),
        std::forward<ExecutionContext>(context), identity,
        std::forward<Args>(args)...);
}

template<
    typename ExecutionContext, typename Function, typename... Args,
    std::enable_if_t<
        IsInvocableWithoutExecutionContext<Function, Args...>, int> = 0>
decltype(auto) invokeWithExecutionContextIfPossible(
    Function&& function, ExecutionContext&&, const Identity& identity,
    Args&&... args)
{
    return std::invoke(
        std::forward<Function>(function), identity,
        std::forward<Args>(args)...);
}

}

/// \endcond

/** \brief Class that adapts a function into MessageHandler interface
 *
 * BasicFunctionMessageHandler wraps a function in a BasicMessageHandler
 * interface. It is responsible for deserializing the message parameters
 * consisting of key–value pairs and forwarding them as arguments to the
 * function in type safe manner. The deserialization is controlled by
 * serialization policy. When the function returns, the return is serialized and
 * passed back to the caller.
 *
 * There are two possible signatures that the function can support. It may be
 * invocable with an Identity object followed by additional arguments, in which
 * case the Identity object received from the caller is passed to the
 * function. Alternatively the Identity parameter may additionally be preceded
 * by the execution context for the message handler, in which case both the
 * execution context and identity are passed.
 *
 * The function must return an object of type \ref Reply. Depending on the type
 * of the reply (ReplySuccess or ReplyFailure) the message handler sets the
 * response status. In case of successful reply, any arguments of ReplySuccess
 * are serialized and added as key–value pair frames to the response object.
 *
 * The following is an example of message handler function in a hypothetical
 * service that checks credentials of a client and returns list of files if the
 * check passes.
 *
 * \code{.cc}
 * Reply<std::vector<std::string>> getFiles(
 *     const Identity& identity, const Blob& credentials)
 * {
 *     if (checkCredentials(identity, credentials)) {
 *         return success(getFilesForUser(identity));
 *     }
 *     return failure();
 * }
 * \endcode
 *
 * BasicFunctionMessageHandler supports optional arguments. If any of the \p
 * Args decays into \c std::optional<T> for some type \c T, the corresponding
 * key–value pair may be omitted from the message. If it is present, the value
 * is deserializez as \c T (and not \c std::optional<T> itself). Similarly, if
 * the reply contains any empty optional values, they are not present in the
 * serialized reply.
 *
 * \tparam ExecutionPolicy Execution policy of the message handler, see \ref
 * executionpolicy
 *
 * \tparam SerializationPolicy See \ref serializationpolicy
 *
 * \tparam Args The types of the arguments used when calling the function
 * (excluding the first identity parameter). This needs to be specified to
 * distinguish between different possible calling signatures provided by the
 * function.
 *
 * \sa makeMessageHandler(), FunctionMessageHandler
 */
template<
    typename ExecutionPolicy, typename Function, typename SerializationPolicy,
    typename... Args>
class BasicFunctionMessageHandler :
    public BasicMessageHandler<ExecutionPolicy> {
public:

    /** \brief Create function message handler
     *
     * \param function the function used to execute the action
     * \param serializer the SerializationPolicy object used to serialize and
     * deserialize strings
     * \param keys tuple containing the keys of the parameters
     * \param replyKeys tuple containing the keys of the parameters of the
     * reply
     *
     * \note The \p keys must appear in the tuple in the same order as the
     * corresponding arguments appear in the \p function signature. The \p
     * replyKeys must appear in the tuple in the same order as the
     * corresponding arguments appear in the Reply object returned by the \p
     * function. Compile time check is done to check that the size of the
     * tuple corresponds to the number of arguments in each case.
     */
    template<typename Keys, typename ReplyKeys>
    BasicFunctionMessageHandler(
        Function function, SerializationPolicy serializer,
        Keys&& keys, ReplyKeys&& replyKeys);

private:

    using ExecutionContext = typename BasicMessageHandler<
        ExecutionPolicy>::ExecutionContext;

    using ParameterVector = typename BasicMessageHandler<
        ExecutionPolicy>::ParameterVector;

    void doHandle(
        ExecutionContext context, const Identity& identity,
        const ParameterVector& params, Response& response) override;

    template<typename T>
    using ParamWrapper = FunctionMessageHandlerImpl::ParamWrapperImpl<
        std::decay_t<T>>;

    using ResultType = decltype(
        FunctionMessageHandlerImpl::invokeWithExecutionContextIfPossible(
            std::declval<Function>(), std::declval<ExecutionContext>(),
            std::declval<Identity>(), std::declval<std::decay_t<Args>>()...));

    static constexpr auto ARGS_SIZE = sizeof...(Args);
    static constexpr auto REPLY_SIZE =
        std::tuple_size<typename ResultType::Types>::value;

    template<typename ReplyKeys, std::size_t... Ns>
    auto internalMakeKeys(ReplyKeys keys, std::index_sequence<Ns...>);

    template<std::size_t N, typename WrappedType>
    bool internalDeserializeAndWrapArg(ByteSpan from, WrappedType& to);

    template<std::size_t... Ns>
    void internalCallFunction(
        ExecutionContext&& context, const Identity& identity,
        const ParameterVector& params, Response& response,
        std::index_sequence<Ns...>);

    Function function;
    SerializationPolicy serializer;
    std::array<Blob, ARGS_SIZE> argKeys;
    std::array<Blob, REPLY_SIZE> replyKeys;
};

template<
    typename ExecutionPolicy, typename Function, typename SerializationPolicy,
    typename... Args>
template<typename Keys, std::size_t... Ns>
auto BasicFunctionMessageHandler<
    ExecutionPolicy, Function, SerializationPolicy, Args...>::
internalMakeKeys([[maybe_unused]] Keys keys, std::index_sequence<Ns...>)
{
    return std::array<Blob, sizeof...(Ns)> {
        stringToBlob(std::get<Ns>(keys))...
    };
}

template<
    typename ExecutionPolicy, typename Function, typename SerializationPolicy,
    typename... Args>
template<typename Keys, typename ReplyKeys>
BasicFunctionMessageHandler<
    ExecutionPolicy, Function, SerializationPolicy, Args...>::
BasicFunctionMessageHandler(
    Function function, SerializationPolicy serializer, Keys&& keys,
    ReplyKeys&& replyKeys) :
    function(std::move(function)),
    serializer(std::move(serializer)),
    argKeys {
        internalMakeKeys(
            std::forward<Keys>(keys),
            std::index_sequence_for<Args...> {})},
    replyKeys {
        internalMakeKeys(
            std::forward<ReplyKeys>(replyKeys),
            std::make_index_sequence<REPLY_SIZE> {})}
{
    static_assert(
        sizeof...(Args) == std::tuple_size<std::decay_t<Keys>>::value,
        "Number of keys must match the number of arguments");
    static_assert(
        REPLY_SIZE == std::tuple_size<std::decay_t<ReplyKeys>>::value,
        "Number of reply keys must match the number of arguments in the reply");
}

template<
    typename ExecutionPolicy, typename Function, typename SerializationPolicy,
    typename... Args>
template<std::size_t N, typename WrappedType>
bool BasicFunctionMessageHandler<
    ExecutionPolicy, Function, SerializationPolicy, Args...>::
internalDeserializeAndWrapArg(ByteSpan from, WrappedType& to)
{
    using ArgType = typename std::tuple_element<N, std::tuple<Args...>>::type;
    using DeserializedType = typename ParamWrapper<ArgType>::DeserializedType;
    if (from.data() == nullptr) {
        return ParamWrapper<ArgType>::optional;
    }
    auto&& deserialized_param =
        serializer.template deserialize<DeserializedType>(from);
    ParamWrapper<ArgType>::wrap(std::move(deserialized_param), to);
    return true;
}

template<
    typename ExecutionPolicy, typename Function, typename SerializationPolicy,
    typename... Args>
template<std::size_t... Ns>
void BasicFunctionMessageHandler<
    ExecutionPolicy, Function, SerializationPolicy, Args...>::
internalCallFunction(
    ExecutionContext&& context, const Identity& identity,
    const ParameterVector& params, Response& response,
    std::index_sequence<Ns...>)
{
    auto params_to_deserialize = std::array<ByteSpan, ARGS_SIZE> {};
    auto first = params.begin();
    const auto last = params.end();
    while (first != last) {
        const auto arg_key_iter = std::find(
            argKeys.begin(), argKeys.end(), *first);
        ++first;
        if (first == last) {
            response.setStatus(REPLY_FAILURE);
            return;
        }
        if (arg_key_iter != argKeys.end()) {
            const auto n = static_cast<std::size_t>(
                arg_key_iter - argKeys.begin());
            assert(n < params_to_deserialize.size());
            params_to_deserialize[n] = *first;
        }
        ++first;
    }
    // Wrapped parameters are deserialized parameters, but wrapped in
    // std::optional to account for the fact that they may not be present in the
    // ParameterVector.  If parameter type is std::optional<T>, then its already
    // wrapped and optional also in ParameterVector.
    [[maybe_unused]] auto wrapped_params =
        std::tuple<typename ParamWrapper<Args>::WrappedType...> {};
    auto all_valid = false;
    try {
        all_valid = ( internalDeserializeAndWrapArg<Ns>(
            std::get<Ns>(params_to_deserialize),
            std::get<Ns>(wrapped_params)) && ... );
    } catch (const SerializationFailureException&) {
        // all_valid remains false
    }
    if (!all_valid) {
        response.setStatus(REPLY_FAILURE);
        return;
    }
    // Call the function. We need to unwrap the parameters. If function excepts
    // std::optional<T>, then unwrapping does nothing.
    using namespace FunctionMessageHandlerImpl;
    auto&& result = invokeWithExecutionContextIfPossible(
        function, std::move(context), identity,
        std::move(ParamWrapper<Args>::unwrap(std::get<Ns>(wrapped_params)))...);
    std::visit(
        ReplyVisitor { serializer, response, replyKeys }, result.reply);
}

template<
    typename ExecutionPolicy, typename Function, typename SerializationPolicy,
    typename... Args>
void BasicFunctionMessageHandler<
    ExecutionPolicy, Function, SerializationPolicy, Args...>::doHandle(
    ExecutionContext context, const Identity& identity,
    const ParameterVector& params, Response& response)
{
    internalCallFunction(
        std::move(context), identity, params, response,
        std::index_sequence_for<Args...>());
}

/** \brief Function message handler with synchronous execution policy
 */
template<typename Function, typename SerializationPolicy, typename... Args>
using FunctionMessageHandler = BasicFunctionMessageHandler<
    SynchronousExecutionPolicy, Function, SerializationPolicy, Args...>;

/** \brief Utility for wrapping function into message handler
 *
 * This is utility for constructing BasicFunctionMessageHandler with some types
 * deducted.
 *
 * \note Invocation argument types cannot be deduced for general function
 * object, which is why \p Args must be explicitly provided for this overload.
 *
 * \tparam Args the types of the arguments of the function, excluding the
 * leading identity string
 *
 * \param function the function to be wrapped
 * \param serializer the serialization policy used by the handler
 * \param keys tuple containing the keys corresponding to the parameters
 * \param replyKeys tuple containing the keys corresponding to the parameters
 * of the reply
 *
 * \return pointer the constructed message handler
 *
 * \sa BasicFunctionMessageHandler
 */
template<
    typename ExecutionPolicy, typename... Args, typename Function,
    typename SerializationPolicy, typename Keys = std::tuple<>,
    typename ReplyKeys = std::tuple<>>
auto makeMessageHandler(
    Function&& function, SerializationPolicy&& serializer, Keys&& keys = {},
    ReplyKeys&& replyKeys = {})
{
    return std::make_unique<
        BasicFunctionMessageHandler<
            ExecutionPolicy, Function, SerializationPolicy, Args...>>(
            std::forward<Function>(function),
            std::forward<SerializationPolicy>(serializer),
            std::forward<Keys>(keys),
            std::forward<ReplyKeys>(replyKeys));
}

/** \brief Utility for wrapping member function call into message handler
 *
 * This function can be used to create BasicFunctionMessageHandler in the
 * typical case that the function is a method invocation for a known
 * object. This overload is for member functions that don’t accept execution
 * context of the handler as their argument.
 *
 * \note The MessageHandler returned by this function stores reference to \p
 * handler. It is the responsibility of the user of this function to ensure
 * that the lifetime of \p handler exceeds the lifetime of the message
 * handler.
 *
 * \param handler the handler object the method call is bound to
 * \param memfn pointer to the member function handling the message
 * \param serializer the serialization policy used by the handler
 * \param keys tuple containing the keys corresponding to the parameters
 * \param replyKeys tuple containing the keys corresponding to the parameters
 * of the reply
 *
 * \return Pointer the constructed message handler
 *
 * \sa BasicFunctionMessageHandler
 */
template<
    typename ExecutionPolicy = SynchronousExecutionPolicy, typename Handler,
    typename Reply, typename... Args, typename SerializationPolicy,
    typename Keys = std::tuple<>, typename ReplyKeys = std::tuple<>>
auto makeMessageHandler(
    Handler& handler, Reply (Handler::*memfn)(const Identity&, Args...),
    SerializationPolicy&& serializer, Keys&& keys = {},
    ReplyKeys&& replyKeys = {})
{
    // Identity always comes as const Identity& from
    // FunctionMessageHandler::doHandle so no need to move/forward
    // it. However, other parameters are constructed in doHandle and moved to
    // the handler function, so there might be performance to be gained by
    // accepting them as rvalue references.
    return makeMessageHandler<ExecutionPolicy, Args...>(
        [&handler, memfn](
            const Identity& identity, std::decay_t<Args>&&... args)
        {
            return (handler.*memfn)(identity, std::move(args)...);
        },
        std::forward<SerializationPolicy>(serializer),
        std::forward<Keys>(keys),
        std::forward<ReplyKeys>(replyKeys));
}

/** \brief Utility for wrapping member function call into message handler
 *
 * This function can be used to create BasicFunctionMessageHandler in the
 * typical case that the function is a method invocation for a known
 * object. This overload is for member functions that accept execution context
 * of the handler as the first argument (before identity).
 *
 * \note The MessageHandler returned by this function stores reference to \p
 * handler. It is the responsibility of the user of this function to ensure
 * that the lifetime of \p handler exceeds the lifetime of the message
 * handler.
 *
 * \param handler the handler object the method call is bound to
 * \param memfn pointer to the member function handling the message
 * \param serializer the serialization policy used by the handler
 * \param keys tuple containing the keys corresponding to the parameters
 * \param replyKeys tuple containing the keys corresponding to the parameters
 * of the reply
 *
 * \return Pointer the constructed message handler
 *
 * \sa BasicFunctionMessageHandler
 */
template<
    typename ExecutionPolicy = SynchronousExecutionPolicy, typename Handler,
    typename Reply, typename ExecutionContext, typename... Args,
    typename SerializationPolicy, typename Keys = std::tuple<>,
    typename ReplyKeys = std::tuple<>>
auto makeMessageHandler(
    Handler& handler,
    Reply (Handler::*memfn)(ExecutionContext, const Identity&, Args...),
    SerializationPolicy&& serializer, Keys&& keys = {},
    ReplyKeys&& replyKeys = {})
{
    return makeMessageHandler<ExecutionPolicy, Args...>(
        [&handler, memfn](
            ExecutionContext context, const Identity& identity,
            std::decay_t<Args>&&... args)
        {
            return (handler.*memfn)(std::move(context), identity,
                std::move(args)...);
        },
        std::forward<SerializationPolicy>(serializer),
        std::forward<Keys>(keys),
        std::forward<ReplyKeys>(replyKeys));
}

}
}

#endif // MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
