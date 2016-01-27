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

    /** \brief Vector containing parameters for the reply message
     *
     * \sa operator()()
     */
    using ReturnValue = std::vector<std::string>;

    virtual ~MessageHandler() = default;

    /** \brief Execute action of this handler
     *
     * The action depends on the implementation of the interface
     *
     * \tparam ParameterIterator Random access iterator for iterating the
     * parameters (convertible to strings)
     *
     * \param first iterator to the first parameter
     * \param last iterator to the last parameter
     *
     * \return ReturnValue containing parameters for the reply message
     *
     * \todo Could the iterator category be weakened?
     */
    template<typename ParameterIterator>
    ReturnValue operator()(ParameterIterator first, ParameterIterator last);

protected:

    /** \brief Parameter to handle()
     */
    using ParameterRange = boost::any_range<
        std::string, boost::random_access_traversal_tag>;

private:

    /** \brief Handle action of this handler
     *
     * \param params range containing the parameters passed to operator()()
     *
     * \return ReturnValue to be returned by operator()()
     *
     * \sa operator()()
     */
    virtual ReturnValue handle(ParameterRange params) = 0;
};

template<typename ParameterIterator>
MessageHandler::ReturnValue MessageHandler::operator()(
    ParameterIterator first, ParameterIterator last)
{
    return handle(ParameterRange(first, last));
}


}
}


#endif // MESSAGING_MESSAGEHANDLER_HH_
