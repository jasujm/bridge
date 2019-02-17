/** \file
 *
 * \brief Definition of Bridge::Messaging::DispatchingMessageHandler class
 */

#ifndef MESSAGING_DISPATCHINGMESSAGEHANDLER_HH_
#define MESSAGING_DISPATCHINGMESSAGEHANDLER_HH_

#include "messaging/MessageHandler.hh"
#include "Blob.hh"
#include "Utility.hh"

#include <map>
#include <memory>

namespace Bridge {
namespace Messaging {

/** \brief Message handler for dispatching based on a parameter
 *
 * A dispatching message handler is a MessageHandler object that dispatches
 * messages to other MessageHandler objects (delegates) by matching a
 * parameter. It accepts frames containing key–value pairs, searches for a known
 * key and looks into its internal mapping for a delegates corresponding to the
 * value. If found, it calls the delegate handler with the original parameters.
 *
 * \tparam DispatchArgument Type of the argument used for parameter matching
 * \tparam SerializationPolicy See \ref serializationpolicy
 */
template<typename DispatchArgument, typename SerializationPolicy>
class DispatchingMessageHandler : public MessageHandler {
public:

    /** \brief Mapping between matched argument and its handler
     */
    using DelegateMap =
        std::map<DispatchArgument, std::shared_ptr<MessageHandler>>;

    /** \brief Create new dispatching message handler
     *
     * \param dispatchKey the key of the parameter used for matching
     * \param serializer the serialization policy used to deserialize the
     * matched argument
     * \param delegates initial map of delegates
     */
    DispatchingMessageHandler(
        Blob dispatchKey, SerializationPolicy serializer,
        DelegateMap delegates = {});

    /** \brief Try setting new delegate
     *
     * \param dispatchArgument the value of the matching parameter for the
     * handler
     * \param delegate the delegate message handler
     */
    bool trySetDelegate(
        const DispatchArgument& dispatchArgument,
        std::shared_ptr<MessageHandler> delegate);

private:

    void doHandle(
        SynchronousExecutionPolicy& execution, const Identity& identity,
        const ParameterVector& params, Response& response) override;

    Blob dispatchKey;
    SerializationPolicy serializer;
    DelegateMap delegates;
};

template<typename DispatchArgument, typename SerializationPolicy>
DispatchingMessageHandler<DispatchArgument, SerializationPolicy>::
DispatchingMessageHandler(
    Blob dispatchKey, SerializationPolicy serializer,
    DelegateMap delegates) :
    dispatchKey {std::move(dispatchKey)},
    serializer {std::move(serializer)},
    delegates {std::move(delegates)}
{
}

template<typename DispatchArgument, typename SerializationPolicy>
void DispatchingMessageHandler<DispatchArgument, SerializationPolicy>::
doHandle(
    SynchronousExecutionPolicy& execution, const Identity& identity,
    const ParameterVector& params, Response& response)
{
    for (auto i : to(params.size(), 2u)) {
        if (params[i] == dispatchKey) {
            if (i+1 < params.size()) {
                auto&& value = serializer.template deserialize<
                    DispatchArgument>(params[i+1]);
                if (const auto iter = delegates.find(value);
                    iter != delegates.end()) {
                    dereference(iter->second).handle(
                        execution, identity, params.begin(), params.end(),
                        response);
                    return;
                }
            }
        }
    }
    response.setStatus(REPLY_FAILURE);
}

template<typename DispatchArgument, typename SerializationPolicy>
bool DispatchingMessageHandler<DispatchArgument, SerializationPolicy>::
trySetDelegate(
    const DispatchArgument& dispatchArgument,
    std::shared_ptr<MessageHandler> delegate)
{
    const auto result = delegates.try_emplace(
        dispatchArgument, std::move(delegate));
    return result.second;
}

/** \brief Helper for creating dispatching message handler
 *
 * \param dispatchKey the key of the parameter used for matching
 * \param serializer the serialization policy used to deserialize the
 * matched argument
 * \param delegates initial map of delegates
 *
 * \return shared_ptr to DispatchingMessageHandler object created with the given
 * parameters
 */
template<typename DispatchArgument, typename SerializationPolicy>
auto makeDispatchingMessageHandler(
    Blob dispatchKey, SerializationPolicy serializer,
    typename DispatchingMessageHandler<DispatchArgument, SerializationPolicy>
    ::DelegateMap delegates = {})
{
    return std::make_shared<
        DispatchingMessageHandler<DispatchArgument, SerializationPolicy>>(
            std::move(dispatchKey), std::move(serializer),
            std::move(delegates));
}

}
}

#endif // MESSAGING_DISPATCHINGMESSAGEHANDLER_HH_
