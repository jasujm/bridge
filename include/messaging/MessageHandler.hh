/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageHandler interface
 */

#ifndef MESSAGING_MESSAGEHANDLER_HH_
#define MESSAGING_MESSAGEHANDLER_HH_

#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace Bridge {
namespace Messaging {

/** \brief Interface for handling messages
 *
 * In RPC protocol, MessageHandler acts as an interface between a driver
 * (e.g. MessageQueue object) that receives messages from and sends replies to
 * a client or a peer. The driver passes identity of the sender and any
 * arguments to the command to MessageHandler and excepts success status it
 * can communicate back to the sender.
 */
class MessageHandler {
public:

    virtual ~MessageHandler() = default;

    /** \brief Handle message
     *
     * \tparam ParameterIterator Input iterator that, when dereferenced,
     * returns an object convertible to string.
     *
     * \param identity the identity of the sender of the message
     * \param first iterator to the first parameter of the message
     * \param last iterator one past the last parameter of the message
     * \param out output iterator to which the output of the message handler
     * is written to
     *
     * \return true if the message was handled successfully, false otherwise
     */
    template<typename OutputIterator, typename ParameterIterator>
    bool handle(
        const std::string& identity,
        ParameterIterator first, ParameterIterator last, OutputIterator out);

protected:

    /** \brief Input parameter to doHandle()
     */
    using ParameterVector = std::vector<std::string>;

    /** \brief Output parameter to doHandle()
     */
    using OutputSink = std::function<void(std::string)>;

private:

    /** \brief Handle action of this handler
     *
     * \param identity the identity of the sender of the message
     * \param params vector containing the parameters passed to handle()
     * \param sink sink that can be invoked to write to the output passed to
     * handle()
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \sa handle()
     */
    virtual bool doHandle(
        const std::string& identity, const ParameterVector& params,
        OutputSink sink) = 0;
};

template<typename OutputIterator, typename ParameterIterator>
bool MessageHandler::handle(
    const std::string& identity,
    ParameterIterator first, ParameterIterator last, OutputIterator out)
{
    return doHandle(
        identity, ParameterVector(first, last), [&out](std::string output)
        {
            *out++ = std::move(output);
        });
}

}
}

#endif // MESSAGING_MESSAGEHANDLER_HH_
