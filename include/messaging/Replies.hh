/** \file
 *
 * \brief Definition of standard replies to messages
 */

#ifndef MESSAGING_REPLIES_HH_
#define MESSAGING_REPLIES_HH_

#include <boost/endian/conversion.hpp>
#include <boost/optional/optional.hpp>

#include <iterator>
#include <string>

namespace Bridge {
namespace Messaging {

/**
 * \brief Status code used in replies
 *
 * In the \ref bridgeprotocol replies to commands are accompanied with status
 * code which is a four byte signed integer. Negative status codes indicate
 * failure and non‐negative codes represent success. Status codes can also
 * encode command specific information.
 *
 * \sa getStatusCode()
 */
using StatusCode = std::int32_t;

/** \brief Generic successful status code
 *
 * \note The particular value contains characters OK if dumped as ASCII
 */
constexpr StatusCode REPLY_SUCCESS = 20299;

/** \brief Generic failed status code
 *
 * \note The particular value contains characters ERR if dumped as ASCII
 */
constexpr StatusCode REPLY_FAILURE = -12234158;

/**
 * \brief Determine if status code is successful
 *
 * \param code the status code
 *
 * \return true if code and *code >= 0, false otherwise
 */
bool isSuccessful(boost::optional<StatusCode> code);

/**
 * \brief Interpret status code
 *
 * The input to this function is a byte string encoding a four byte big endian
 * integer. This function is used to convert the string into a native StatusCode
 * value.
 *
 * \param status status code as byte string
 *
 * \return status code, or none if the input does not contain four bytes
 */
template<typename MessageString>
boost::optional<StatusCode> getStatusCode(const MessageString& status)
{
    const auto d = std::data(status);
    const auto s = std::size(status) * sizeof(*d);
    if (s == sizeof(StatusCode)) {
        auto ret = StatusCode {};
        std::memcpy(&ret, d, s);
        return boost::endian::big_to_native(ret);
    }
    return boost::none;
}

/** \brief Determine whether or not message is a successful reply
 *
 * A successful reply begins with a status frame containing positive status
 * code, and a frame containing the command the reply is for. Because the user
 * of this method could be waiting for reply to one of several commands, this
 * function does not check for specific command but instead returns iterator to
 * the frame, letting the caller do further checking.
 *
 * \note The possible initial empty frame must be stripped before passing the
 * message to this function. For example recvAll() does the stripping
 * automatically.
 *
 * \tparam MessageIterator An input iterator that, when dereferenced, returns a
 * string containing part of the message.
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
    const auto code = getStatusCode(*first);
    if (!isSuccessful(code)) {
        return last;
    }
    ++first;
    return first;
}

}
}

#endif // MESSAGING_REPLIES_HH_
