/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageQueue class
 */

#ifndef MESSAGING_MESSAGEQUEUE_HH_
#define MESSAGING_MESSAGEQUEUE_HH_

#include <any>
#include <cassert>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/noncopyable.hpp>
#include <zmq.hpp>

#include "messaging/Identity.hh"
#include "messaging/MessageHandler.hh"
#include "Blob.hh"
#include "BlobMap.hh"
#include "Utility.hh"

namespace Bridge {

/** \brief The messaging framework
 *
 * Namespace Messaging contains utilities for exchanging messages between
 * different threads and processes in the Bridge application.
 */
namespace Messaging {

/** \brief Message queue for communicating between threads and processes
 *
 * MessageQueue is an object receiving messages from sockets, dispatching the
 * message to correct message handler and replying. The MessageQueue object does
 * not own the sockets used to receive and send the messages. Instead
 * MessageQueue is designed to be used with MessageLoop that handles the actual
 * polling of the sockets.
 *
 * MessageQueue uses request–reply pattern (although the sockets used do not
 * have to be rep sockets). The messages sent to the message queue are commands
 * with arbitrary number of arguments to be handled by the handler for the
 * command. A recognized command (i.e. command for which a handler is
 * registered) causes the handler for that command to be executed. Based on the
 * result of the handling either success or failure is reported.
 *
 * MessageQueue does not interpret commands or arguments in any way, but handles
 * them as arbitrary byte sequences. Matching the command is done by binary
 * comparison.
 */
class MessageQueue : private boost::noncopyable {
public:

    /** \brief Create message queue with no handlers
     */
    MessageQueue();

    /** \brief Create message queue
     *
     * \param handlers initial handlers
     */
    MessageQueue(
        std::initializer_list<
            std::pair<ByteSpan, std::shared_ptr<MessageHandler>>> handlers);

    ~MessageQueue();

    /** \brief Add execution policy
     *
     * If there is no execution policy of type \c ExecutionPolicy, registers \p
     * execution to be used with any BasicMessageHandler object using \c
     * ExecutionPolicy, and returns true. Otherwise the call has no effect and
     * returns false.
     *
     * \param executionPolicy the execution policy to store
     *
     * \return true if \p execution was successfully registered, false otherwise
     */
    template<typename ExecutionPolicy>
    bool addExecutionPolicy(ExecutionPolicy executionPolicy);

    /** \brief Try to set handler for a command
     *
     * If there is no handler for \p command, the handler is assigned and the
     * call returns true. Otherwise the call has no effect and returns false.
     *
     * The execution policy the handler expects needs to be set using
     * addExecutionPolicy() before the handler is added. If \c ExecutionPolicy
     * is default constructible, it is created if missing.
     *
     * \return true if \p handler was successfully registered, false otherwise
     */
    template<
        typename MessageHandlerType,
        typename = std::void_t<typename MessageHandlerType::ExecutionPolicyType>>
    bool trySetHandler(
        ByteSpan command, std::shared_ptr<MessageHandlerType> handler);

    /** \brief Receive and reply the next message
     *
     * When called, the method receives a message from \p socket, dispatches it
     * to the correct handler and sends the reply through \p socket. The reply
     * consists of a status frame, the echoed command frame and optional reply
     * argument frames, as determined by the message handler. If there is no
     * handler for the command, REPLY_FAILURE is sent with the echoed command
     * frame.
     *
     * When dispatching the command, the MessageQueue object creates a copy of
     * the execution policy object expected by the registered
     * BasicMessageHandler object, provided that one has been registered by
     * addExecutionPolicy() or can be default constructed by the MessageQueue
     * object. It then invokes the message handler as specified by the execution
     * policy, passing it the Response object and command argument frames.
     *
     * \note The intention is that MessageQueue acts as a “server” receiving
     * messages and replying back to the “client” who sent the message. The
     * type of \p socket should support the intended use. For example rep,
     * pair or router are such socket types.
     *
     * \note MessageQueue passes the identity received from the socket as
     * argument to the MessageHandler instances. If \p socket is not router, the
     * Identity::routingId is always an empty blob.
     *
     * \param socket the socket the message is received from and the reply is
     * sent to
     */
    void operator()(zmq::socket_t& socket);

private:

    using MessageVector = std::vector<zmq::message_t>;

    class BasicResponse : public Response {
    public:
        BasicResponse(MessageVector&, std::ptrdiff_t);
        void sendResponse(zmq::socket_t&);

    private:
        void handleSetStatus(StatusCode) override;
        void handleAddFrame(ByteSpan) override;

        std::ptrdiff_t nStatusFrame;
        MessageVector frames;
    };

    template<typename ExecutionPolicy>
    auto internalAddExecutionPolicyHelper(ExecutionPolicy executionPolicy);

    template<typename ExecutionPolicy>
    auto internalCreateExecutor(
        std::shared_ptr<BasicMessageHandler<ExecutionPolicy>> handler);

    using ExecutionFunction = std::function<
        void(Identity&&, MessageVector&&, std::ptrdiff_t, zmq::socket_t&)>;

    std::map<std::type_index, std::any> policies;
    BlobMap<ExecutionFunction> executors;
    ExecutionFunction defaultExecutor;
};

template<typename ExecutionPolicy>
auto MessageQueue::internalAddExecutionPolicyHelper(
    ExecutionPolicy executionPolicy)
{
    return policies.try_emplace(
        typeid(ExecutionPolicy), std::move(executionPolicy));
}

template<typename ExecutionPolicy>
bool MessageQueue::addExecutionPolicy(ExecutionPolicy executionPolicy)
{
    return internalAddExecutionPolicyHelper(std::move(executionPolicy)).second;
}

template<typename ExecutionPolicy>
auto MessageQueue::internalCreateExecutor(
    std::shared_ptr<BasicMessageHandler<ExecutionPolicy>> handler)
{
    // ExecutionFunction is a type erased function used to create execution
    // policy for the MessageHandler object and then executing it
    // - First of all if the execution policy has been registered, use the
    //   factory function and any_cast to the correct ExecutionPolicy type (see
    //   the comment in addExecutionPolicy())
    // - If the execution policy is not set, but the execution policy is default
    //   constructible, default construct it
    // - Otherwise we're screwed and might as well throw an exception
    const auto& policy_type_id = typeid(ExecutionPolicy);
    auto policy_iter = policies.find(policy_type_id);
    if (policy_iter == policies.end()) {
        if constexpr (std::is_default_constructible_v<ExecutionPolicy>) {
            policy_iter =
                internalAddExecutionPolicyHelper(ExecutionPolicy {}).first;
        } else {
            throw std::runtime_error {"Execution policy missing"};
        }
    };
    return ExecutionFunction {
        [&policy = std::any_cast<ExecutionPolicy&>(policy_iter->second),
         handler = std::move(handler)](
             Identity&& identity, MessageVector&& inputFrames,
             const std::ptrdiff_t nPrefix, zmq::socket_t& socket)
        {
            std::invoke(
                policy,
                [identity = std::move(identity),
                 inputFrames = std::move(inputFrames), nPrefix, &socket,
                 &handler = dereference(handler)](auto&& context) mutable
                {
                    const auto command_frame_iter = inputFrames.begin() + nPrefix;
                    const auto last_input_frame_iter = inputFrames.end();
                    assert(command_frame_iter != last_input_frame_iter);
                    auto response = BasicResponse(inputFrames, nPrefix);
                    handler.handle(
                        std::forward<decltype(context)>(context), identity,
                        command_frame_iter+1, last_input_frame_iter, response);
                    response.sendResponse(socket);
                });
        }
    };
}

template<
    typename MessageHandlerType,
    typename = std::void_t<typename MessageHandlerType::ExecutionPolicy>>
bool MessageQueue::trySetHandler(
    const ByteSpan command, std::shared_ptr<MessageHandlerType> handler)
{
    return executors.emplace(
        Blob(command.begin(), command.end()),
        internalCreateExecutor<typename MessageHandlerType::ExecutionPolicyType>(
            std::move(handler))).second;
}

}
}

#endif // MESSAGING_MESSAGEQUEUE_HH_
