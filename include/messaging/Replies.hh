/** \file
 *
 * \brief Definition of standard replies to messages
 */

#include <iterator>
#include <string>

namespace Bridge {
namespace Messaging {

/** \brief Message used as reply to indicate successful request
 */
extern const std::string REPLY_SUCCESS;

/** \brief Message used as reply to indicate failure to fulfill the request
 */
extern const std::string REPLY_FAILURE;

/** \brief Determine whether or not message is successful reply
 *
 * A successful reply begins with REPLY_SUCCESS frame and frame containing the
 * \p command the reply is for.
 *
 * \note The possible initial empty frame must be stripped before passing the
 * message to this function. For example recvAll() does the stripping
 * automatically.
 *
 * \tparam MessageIterator An input iterator that, when dereferenced, returns
 * a string containing subsequent parts of the message.
 *
 * \param first iterator to the first part of the message examined
 * \param last iterator one past the last part of the message examined
 * \param command the command the reply is checked for
 *
 * \return true if the message is successful reply to \p command, false
 * otherwise
 */
template<typename MessageIterator, typename MessageString>
bool isSuccessfulReply(
    MessageIterator first, MessageIterator last, const MessageString& command)
{
    if (first == last || *first != REPLY_SUCCESS) {
        return false;
    }
    ++first;
    if (first == last || *first != command) {
        return false;
    }
    ++first;
}

}
}
