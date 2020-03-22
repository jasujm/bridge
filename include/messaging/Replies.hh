/** \file
 *
 * \brief Definition of standard replies to messages
 */

#ifndef MESSAGING_REPLIES_HH_
#define MESSAGING_REPLIES_HH_

#include "messaging/MessageUtility.hh"
#include "messaging/Sockets.hh"
#include "Blob.hh"

namespace Bridge {
namespace Messaging {

/** \brief Generic successful status code
 *
 * \note The particular value contains characters OK if dumped as ASCII
 */
extern const ByteSpan REPLY_SUCCESS;

/** \brief Generic failed status code
 *
 * \note The particular value contains characters ERR if dumped as ASCII
 */
extern const ByteSpan REPLY_FAILURE;

/**
 * \brief Determine if a status code is successful
 *
 * \param status the status code
 *
 * \return true if \p status has prefix OK, false otherwise
 */
bool isSuccessful(ByteSpan status);

/** \brief Determine whether or not a multipart message is a successful reply
 *
 * A successful reply begins with a status frame containing successful
 * status code, and a frame containing the command the reply is
 * for. Because the user of this method could be waiting for reply to
 * one of several commands, this function does not check for specific
 * command but instead returns iterator to the frame, letting the
 * caller do further checking.
 *
 * \note The range passed to this function must not contain the initial empty
 * frame in case of message received through dealer/router socket. The first
 * frame is interpreted as status code.
 *
 * \tparam MessageIterator An input iterator that, when dereferenced, returns a
 * Message objects.
 *
 * \param first iterator to the first part of the message examined
 * \param last iterator one past the last part of the message examined
 *
 * \return If the reply is successful, iterator to the part containing the
 * command, or \p last otherwise
 */
template<typename MessageIterator>
auto isSuccessfulReply(MessageIterator first, MessageIterator last)
{

    if (first == last) {
        return last;
    }
    if (!isSuccessful(messageView(*first))) {
        return last;
    }
    ++first;
    return first;
}

}
}

#endif // MESSAGING_REPLIES_HH_
