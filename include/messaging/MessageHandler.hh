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

#include <boost/range/any_range.hpp>

namespace Bridge {
namespace Messaging {

/** \brief Interface for handling messages
 *
 * In RPC protocol, MessageHandler acts as an interface between a driver
 * (message queue etc.) that receives messages from and sends replies to a
 * client or a peer. The driver passes identity of the sender and any
 * arguments to the command to MessageHandler and excepts success status it
 * can communicate back to the sender.
 */
class MessageHandler {
public:

    virtual ~MessageHandler() = default;

    /** \brief Execute action of this handler
     *
     * \tparam ParameterIterator Random access iterator for iterating the
     * parameters (convertible to strings)
     *
     * \param identity the identity of the sender of the message
     * \param first iterator to the first parameter
     * \param last iterator one past the last parameter
     * \param out output iterator to which the output of the message handler
     * is written to
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \todo Could the iterator category be weakened?
     */
    template<typename OutputIterator, typename ParameterIterator>
    bool handle(
        const std::string& identity,
        ParameterIterator first, ParameterIterator last, OutputIterator out);

protected:

    /** \brief Input parameter to doHandle()
     */
    using ParameterRange = boost::any_range<
        std::string, boost::random_access_traversal_tag>;

    /** \brief Output parameter to doHandle()
     */
    using OutputSink = std::function<void(std::string)>;

private:

    /** \brief Handle action of this handler
     *
     * \param identity the identity of the sender of the message
     * \param params range containing the parameters passed to handle()
     * \param sink sink that can be invoked to write to the output passed to
     * handle()
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \sa handle()
     */
    virtual bool doHandle(
        const std::string& identity, ParameterRange params,
        OutputSink sink) = 0;
};

template<typename OutputIterator, typename ParameterIterator>
bool MessageHandler::handle(
    const std::string& identity,
    ParameterIterator first, ParameterIterator last, OutputIterator out)
{
    return doHandle(
        identity, ParameterRange(first, last), [&out](std::string output)
        {
            *out++ = std::move(output);
        });
}

}
}

#endif // MESSAGING_MESSAGEHANDLER_HH_
