/** \file
 *
 * \brief Definition of Bridge::Messaging::FunctionMessageHandler class
 */

#ifndef MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
#define MESSAGING_FUNCTIONMESSAGEHANDLER_HH_

#include "messaging/Identity.hh"
#include "messaging/MessageHandler.hh"
#include "messaging/SerializationFailureException.hh"
#include "Blob.hh"
#include "BlobMap.hh"

#include <array>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <string>
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

/** \brief Class that adapts a function into MessageHandler interface
 *
 * FunctionMessageHandler wraps a function into MessageHandler interface. It
 * is responsible for deserializing the message parameters consisting of
 * key–value pairs and forwarding them as arguments to the function in type
 * safe manner. The deserialization is controlled by serialization
 * policy. When the function returns, the return is serialized and passed back
 * to the caller.
 *
 * The function that handles the message must accept a string containing the
 * identity and any number of additional arguments. It must return an object
 * of type \ref Reply. Depending on the type of the reply (ReplySuccess or
 * ReplyFailure) the message handler returns true or false to the caller. In
 * case of successful reply, any arguments of ReplySuccess are serialized and
 * written as key–value pairs to the output iterator provided by the caller.
 *
 * The following is an example of message handler function in hypothetical
 * cloud service that checks credentials of a client and returns list of files
 * if the check passes.
 *
 * \code{.cc}
 * Reply<std::vector<std::string>> getFiles(
 *     const Blob& identity, const Blob& credentials)
 * {
 *     if (checkCredentials(identity, credentials)) {
 *         return success(getFilesForUser(identity));
 *     }
 *     return failure();
 * }
 * \endcode
 *
 * FunctionMessageHandler supports optional arguments. If any of the \p Args
 * decays into \c std::optional<T> for some type \c T, the corresponding
 * key–value pair may be omitted from the message. If it is present, the value
 * is deserializez as \c T (and not \c std::optional<T> itself). Similarly,
 * if the reply contains any empty optional values, they are not present in
 * the serialized reply.
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
    FunctionMessageHandler(
        Function function, SerializationPolicy serializer,
        Keys&& keys, ReplyKeys&& replyKeys);

private:

    void doHandle(
        SynchronousExecutionPolicy&, const Identity& identity,
        const ParameterVector& params, Response& response) override;

    template<typename T>
    struct ParamTraitsImplBase {
        using WrappedType = std::optional<T>;
        using DeserializedType = T;
        static void initialize(DeserializedType&& param, WrappedType& wrapper)
        {
            wrapper.emplace(std::move(param));
        }
    };

    template<typename T>
    struct ParamTraitsImpl : public ParamTraitsImplBase<T> {
        using Base = ParamTraitsImplBase<T>;
        using WrappedType = typename Base::WrappedType;
        using DeserializedType = typename Base::DeserializedType;
        static bool isValid(const WrappedType& wrapper)
        {
            return bool(wrapper);
        }
        static decltype(auto) unwrap(WrappedType& wrapper)
        {
            return std::move(*wrapper);
        }
        static bool shouldSerialize(const DeserializedType&)
        {
            return true;
        }
        static const T& getSerializable(const T& value)
        {
            return value;
        }
    };

    template<typename T>
    struct ParamTraitsImpl<std::optional<T>> : public ParamTraitsImplBase<T> {
        using Base = ParamTraitsImplBase<T>;
        using WrappedType = typename Base::WrappedType;
        using DeserializedType = typename Base::DeserializedType;
        static bool isValid(const WrappedType&)
        {
            return true;
        }
        static decltype(auto) unwrap(WrappedType& wrapper)
        {
            return std::move(wrapper);
        }
        static bool shouldSerialize(const std::optional<T>& value)
        {
            return bool(value);
        }
        static const T& getSerializable(const std::optional<T>& value)
        {
            assert(value);
            return *value;
        }
    };

    template<typename T>
    using ParamTraits = ParamTraitsImpl<std::decay_t<T>>;

    using ParamTuple = std::tuple<typename ParamTraits<Args>::WrappedType...>;
    using InitFunction =
        bool (FunctionMessageHandler::*)(ByteSpan, ParamTuple&);
    using InitFunctionMap = BlobMap<InitFunction>;
    using ResultType = typename std::invoke_result_t<
        Function, Identity, typename ParamTraits<Args>::DeserializedType...>;
    static constexpr auto REPLY_SIZE =
        std::tuple_size<typename ResultType::Types>::value;
    using ReplyKeysArray = std::array<std::string, REPLY_SIZE>;

    class ReplyVisitor {
    public:

        ReplyVisitor(
            SerializationPolicy& serializer, Response& response,
            ReplyKeysArray& replyKeys);

        void operator()(ReplyFailure) const;

        template<typename... Args2>
        void operator()(const ReplySuccess<Args2...>& reply) const;

    private:

        template<std::size_t N, typename... Args2>
        void internalReplySuccessHelper(const std::tuple<Args2...>& args) const;

        SerializationPolicy& serializer;
        Response& response;
        ReplyKeysArray& replyKeys;
    };

    template<typename Keys, std::size_t... Ns>
    auto makeInitFunctionMap(Keys keys, std::index_sequence<Ns...>);

    template<typename ReplyKeys, std::size_t... Ns>
    auto makeReplyKeys(ReplyKeys keys, std::index_sequence<Ns...>);

    template<std::size_t N>
    bool internalInitParam(ByteSpan arg, ParamTuple& params);

    template<std::size_t... Ns>
    void internalCallFunction(
        const Identity& identity, const ParameterVector& params,
        Response& response, std::index_sequence<Ns...>)
    {
        auto first = params.begin();
        const auto last = params.end();
        ParamTuple args;
        // Deserialize parameters into function arguments
        while (first != last) {
            const auto iter = initFunctions.find(*first++);
            if (first == last) {
                response.setStatus(REPLY_FAILURE);
                return;
            }
            if (iter != initFunctions.end()) {
                const auto memfn = iter->second;
                const auto success = (this->*memfn)(*first, args);
                if (!success) {
                    response.setStatus(REPLY_FAILURE);
                    return;
                }
            }
            ++first;
        }
        // Check that all required arguments are initialized
        std::array<bool, sizeof...(Args)> all_initialized {{
            ParamTraits<Args>::isValid(std::get<Ns>(args))...}};
        for (const auto initialized : all_initialized) {
            if (!initialized) {
                response.setStatus(REPLY_FAILURE);
                return;
            }
        }
        // Call the function
        auto&& result = function(
            identity,
            ParamTraits<Args>::unwrap(std::get<Ns>(args))...);
        std::visit(
            ReplyVisitor {serializer, response, replyKeys}, result.reply);
    }

    Function function;
    SerializationPolicy serializer;
    InitFunctionMap initFunctions;
    ReplyKeysArray replyKeys;
};

template<typename Function, typename SerializationPolicy, typename... Args>
template<typename ReplyKeys, std::size_t... Ns>
auto FunctionMessageHandler<Function, SerializationPolicy, Args...>::
makeReplyKeys([[maybe_unused]] ReplyKeys replyKeys, std::index_sequence<Ns...>)
{
    return ReplyKeysArray {{
        std::move(std::get<Ns>(replyKeys))...
    }};
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<typename Keys, std::size_t... Ns>
auto FunctionMessageHandler<Function, SerializationPolicy, Args...>::
makeInitFunctionMap([[maybe_unused]] Keys keys, std::index_sequence<Ns...>)
{
    return InitFunctionMap {
        {
            stringToBlob(std::move(std::get<Ns>(keys))),
            &FunctionMessageHandler::internalInitParam<Ns>
        }...
    };
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<typename Keys, typename ReplyKeys>
FunctionMessageHandler<Function, SerializationPolicy, Args...>::
FunctionMessageHandler(
    Function function, SerializationPolicy serializer, Keys&& keys,
    ReplyKeys&& replyKeys) :
    function(std::move(function)),
    serializer(std::move(serializer)),
    initFunctions {
        makeInitFunctionMap(
            std::forward<Keys>(keys),
            std::index_sequence_for<Args...> {})},
    replyKeys {
        makeReplyKeys(
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

template<typename Function, typename SerializationPolicy, typename... Args>
void FunctionMessageHandler<Function, SerializationPolicy, Args...>::doHandle(
    SynchronousExecutionPolicy&, const Identity& identity,
    const ParameterVector& params, Response& response)
{
    internalCallFunction(
        identity, params, response, std::index_sequence_for<Args...>());
}

template<typename Function, typename SerializationPolicy, typename... Args>
FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::ReplyVisitor(
    SerializationPolicy& serializer, Response& response,
    ReplyKeysArray& replyKeys) :
    serializer {serializer},
    response {response},
    replyKeys {replyKeys}
{
}

template<typename Function, typename SerializationPolicy, typename... Args>
void FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::operator()(ReplyFailure) const
{
    response.setStatus(REPLY_FAILURE);
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<typename... Args2>
void FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::operator()(const ReplySuccess<Args2...>& reply) const
{
    response.setStatus(REPLY_SUCCESS);
    if constexpr (0 < sizeof...(Args2)) {
        internalReplySuccessHelper<0>(reply.arguments);
    }
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<std::size_t N, typename... Args2>
void FunctionMessageHandler<Function, SerializationPolicy, Args...>::
ReplyVisitor::internalReplySuccessHelper(const std::tuple<Args2...>& args) const
{
    using ArgType = typename std::tuple_element<N, std::tuple<Args2...>>::type;
    const auto& arg = std::get<N>(args);
    if (ParamTraits<ArgType>::shouldSerialize(arg)) {
        response.addFrame(asBytes(std::get<N>(replyKeys)));
        response.addFrame(asBytes(serializer.serialize(ParamTraits<ArgType>::getSerializable(arg))));
    }
    if constexpr (N+1 < sizeof...(Args2)) {
        internalReplySuccessHelper<N+1>(args);
    }
}

template<typename Function, typename SerializationPolicy, typename... Args>
template<std::size_t N>
bool FunctionMessageHandler<Function, SerializationPolicy, Args...>::
internalInitParam(ByteSpan arg, ParamTuple& params)
{
    using ArgType = typename std::tuple_element<N, std::tuple<Args...>>::type;
    using DeserializedType = typename ParamTraits<ArgType>::DeserializedType;
    try {
        auto&& param = serializer.template deserialize<DeserializedType>(arg);
        ParamTraits<ArgType>::initialize(std::move(param), std::get<N>(params));
        return true;
    }
    catch (const SerializationFailureException&) {
        return false;
    }
}

/** \brief Utility for wrapping function into message handler
 *
 * This is utility for constructing FunctionMessageHandler with some types
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
 * \sa FunctionMessageHandler
 */
template<
    typename... Args, typename Function, typename SerializationPolicy,
    typename Keys = std::tuple<>, typename ReplyKeys = std::tuple<>>
auto makeMessageHandler(
    Function&& function, SerializationPolicy&& serializer, Keys&& keys = {},
    ReplyKeys&& replyKeys = {})
{
    return std::make_unique<
        FunctionMessageHandler<Function, SerializationPolicy, Args...>>(
        std::forward<Function>(function),
        std::forward<SerializationPolicy>(serializer),
        std::forward<Keys>(keys),
        std::forward<ReplyKeys>(replyKeys));
}

/** \brief Utility for wrapping member function call into message handler
 *
 * This function can be used to create FunctionMessageHandler in the usual
 * case that the function is a method invocation for a known object.
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
 * \sa FunctionMessageHandler
 */
template<
    typename Handler, typename Reply, typename... Args,
    typename SerializationPolicy, typename Keys = std::tuple<>,
    typename ReplyKeys = std::tuple<>>
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
    return makeMessageHandler<Args...>(
        [&handler, memfn](
            const Identity& identity, std::decay_t<Args>&&... args)
        {
            return (handler.*memfn)(identity, std::move(args)...);
        },
        std::forward<SerializationPolicy>(serializer),
        std::forward<Keys>(keys),
        std::forward<ReplyKeys>(replyKeys));
}

}
}

#endif // MESSAGING_FUNCTIONMESSAGEHANDLER_HH_
