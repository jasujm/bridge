/** \file
 *
 * \brief Definition of Bridge::Messaging::MessageHandler interface
 */

#ifndef MESSAGING_MESSAGEHANDLER_HH_
#define MESSAGING_MESSAGEHANDLER_HH_

#include <string>
#include <vector>
#include <utility>

#include <boost/range/any_range.hpp>

namespace Bridge {
namespace Messaging {

/** \brief Interface for handling messages
 */
class MessageHandler {
public:

    virtual ~MessageHandler() = default;

    /** \brief Execute action of this handler
     *
     * \tparam ParameterIterator Random access iterator for iterating the
     * parameters (convertible to strings)
     *
     * \param first iterator to the first parameter
     * \param last iterator to the last parameter
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \todo Could the iterator category be weakened?
     */
    template<typename ParameterIterator>
    bool handle(ParameterIterator first, ParameterIterator last);

protected:

    /** \brief Parameter to doHandle()
     */
    using ParameterRange = boost::any_range<
        std::string, boost::random_access_traversal_tag>;

private:

    /** \brief Handle action of this handler
     *
     * \param params range containing the parameters passed to handle()
     *
     * \return true if the message was handled successfully, false otherwise
     *
     * \sa handle()
     */
    virtual bool doHandle(ParameterRange params) = 0;
};

template<typename ParameterIterator>
bool MessageHandler::handle(ParameterIterator first, ParameterIterator last)
{
    return doHandle(ParameterRange(first, last));
}


}
}


#endif // MESSAGING_MESSAGEHANDLER_HH_
